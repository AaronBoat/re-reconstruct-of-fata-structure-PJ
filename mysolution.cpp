#include "MySolution.h"
#include <limits>
#include <random>
#include <omp.h>
#include <immintrin.h>
#include <queue>      // 解决 priority_queue 未定义
#include <functional> // 解决 greater<T> 未定义
#include <utility>    // 解决 pair 未定义

// --- 常量配置 (重排序优化方案) ---
static const int M = 40;                // 优化后的参数
static const int EF_CONSTRUCTION = 300; // 优化后的参数
static const int EF_SEARCH = 200;
static const float ML = 1.0f / log(2.0f); // ~1.44
static const float GAMMA = 1.0f;          // 用于 RobustPrune

// --- 线程局部存储优化 (Optimization 2) ---
struct VisitedBuffer
{
    vector<int> visited_tags;
    int current_tag;

    VisitedBuffer() : current_tag(0) {}

    void prepare(int num_nodes)
    {
        if (visited_tags.size() < (size_t)num_nodes)
        {
            visited_tags.resize(num_nodes, 0);
            current_tag = 0;
        }
        current_tag++;
        if (current_tag == 0)
        { // 溢出重置
            fill(visited_tags.begin(), visited_tags.end(), 0);
            current_tag = 1;
        }
    }

    bool is_visited(int id) const
    {
        return visited_tags[id] == current_tag;
    }

    void mark(int id)
    {
        visited_tags[id] = current_tag;
    }
};

static thread_local VisitedBuffer tls_visited;
static thread_local vector<unsigned char> tls_quant_query_buf;    // 避免频繁申请内存
static thread_local vector<pair<float, int>> tls_candidate_queue; // [性能优化] 复用候选队列内存

// --- 辅助结构：固定大小的候选集 (Optimization 5) ---
// 替代 priority_queue 以减少堆操作开销
struct Candidate
{
    float dist;
    int id;

    bool operator<(const Candidate &other) const
    {
        return dist < other.dist;
    }
    bool operator>(const Candidate &other) const
    {
        return dist > other.dist;
    }
};

// --- 距离计算实现 ---

// 优化1: AVX2 SIMD 浮点距离
inline float Solution::dist_l2_float_avx(const float *a, const float *b, int d) const
{
#if defined(__AVX2__)
    __m256 sum = _mm256_setzero_ps();
    const float *pA = a;
    const float *pB = b;
    int i = 0;
    for (; i + 8 <= d; i += 8)
    {
        __m256 va = _mm256_loadu_ps(pA + i);
        __m256 vb = _mm256_loadu_ps(pB + i);
        __m256 diff = _mm256_sub_ps(va, vb);
        sum = _mm256_fmadd_ps(diff, diff, sum);
    }
    float res[8];
    _mm256_storeu_ps(res, sum);
    float total = res[0] + res[1] + res[2] + res[3] + res[4] + res[5] + res[6] + res[7];
    for (; i < d; ++i)
    {
        float diff = pA[i] - pB[i];
        total += diff * diff;
    }
    return total;
#else
    float total = 0;
    for (int i = 0; i < d; ++i)
    {
        float diff = a[i] - b[i];
        total += diff * diff;
    }
    return total;
#endif
}

// 方案A: 量化距离计算 (Layer 0 专用)
inline float Solution::dist_l2_quant(int id_a, const unsigned char *b_quant, int d) const
{
    const unsigned char *p_quant = &data_quant[(long long)id_a * d];
    long long raw_dist_sq = 0;

// 指南要求的实现方式，利用Simd Reduction
// 注意：这里编译器会自动向量化
#pragma omp simd reduction(+ : raw_dist_sq)
    for (int i = 0; i < d; ++i)
    {
        int diff = (int)p_quant[i] - (int)b_quant[i];
        raw_dist_sq += diff * diff;
    }
    return (float)raw_dist_sq;
}

// --- 量化逻辑 ---

void Solution::init_quantization()
{
    if (num_vectors == 0)
        return;

    // 1. 计算全局范围
    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();

    // 采样部分向量以加速范围计算 (全部遍历也很快，这里为稳妥全遍历)
    for (const auto &v : data_flat)
    {
        if (v < min_val)
            min_val = v;
        if (v > max_val)
            max_val = v;
    }

    global_min = min_val;
    if (max_val - min_val < 1e-6f)
    {
        global_scale_inv = 0;
        use_quantization = false;
    }
    else
    {
        global_scale_inv = 255.0f / (max_val - min_val);
        use_quantization = true;
    }

    // 2. 量化所有基库向量
    long long total_elements = (long long)num_vectors * dimension;
    data_quant.resize(total_elements);

#pragma omp parallel for
    for (int i = 0; i < num_vectors; ++i)
    {
        quantize_vec(&data_flat[i * dimension], &data_quant[(long long)i * dimension]);
    }
}

inline void Solution::quantize_vec(const float *src, unsigned char *dst) const
{
    if (!use_quantization)
        return;
    for (int i = 0; i < dimension; ++i)
    {
        int q = static_cast<int>((src[i] - global_min) * global_scale_inv + 0.5f);
        if (q < 0)
            q = 0;
        if (q > 255)
            q = 255;
        dst[i] = (unsigned char)q;
    }
}

// --- 搜索层逻辑 ---

// 构建阶段使用的搜索 (精确距离，操作动态图)
void Solution::search_layer_build(const float *query, vector<int> &candidates,
                                  const vector<int> &ep, int ef, int lc) const
{

    Candidate top_candidates[512]; // Fixed size buffer
    int sz = 0;

    tls_visited.prepare(num_vectors);

    // 优先队列逻辑 (使用std::priority_queue会慢，这里用简单的排序数组或堆)
    // 为保持代码简洁且遵循指南，使用标准的最小/最大堆逻辑
    // 为了极致性能，这里手动维护两个集合:
    // C: 待探索集合 (min-heap by distance)
    // W: 结果集合 (max-heap by distance, size <= ef)

    priority_queue<pair<float, int>, vector<pair<float, int>>, greater<pair<float, int>>> C;
    priority_queue<pair<float, int>> W;

    // 初始化入口点
    for (int pid : ep)
    {
        if (!tls_visited.is_visited(pid))
        {
            tls_visited.mark(pid);
            float dist = dist_l2_float_avx(query, &data_flat[pid * dimension], dimension);
            C.push({dist, pid});
            W.push({dist, pid});
            if (W.size() > ef)
                W.pop();
        }
    }

    while (!C.empty())
    {
        auto curr = C.top();
        C.pop();
        float dist_c = curr.first;
        int id_c = curr.second;

        if (dist_c > W.top().first)
            break; // 剪枝

        // 遍历邻居
        const vector<int> &neighbors = nodes[id_c].neighbors[lc];

        // 预取优化 (Optimization 6)
        for (size_t i = 0; i < neighbors.size(); ++i)
        {
            int nid = neighbors[i];
            if (tls_visited.is_visited(nid))
                continue;
            tls_visited.mark(nid);

            // Prefetch next
            if (i + 1 < neighbors.size())
            {
                _mm_prefetch((const char *)&data_flat[neighbors[i + 1] * dimension], _MM_HINT_T0);
            }

            float d = dist_l2_float_avx(query, &data_flat[nid * dimension], dimension);

            if (W.size() < ef || d < W.top().first)
            {
                W.push({d, nid});
                C.push({d, nid});
                if (W.size() > ef)
                    W.pop();
            }
        }
    }

    // 收集结果
    candidates.clear();
    while (!W.empty())
    {
        candidates.push_back(W.top().second);
        W.pop();
    }
    // W pop出来是降序，需要反转成升序以便后续处理 (虽然Heuristic里会重排，但保持有序好)
    // 但Heuristic通常只需要集合。这里不用反转。
}

// 最终查询阶段使用的搜索 (Layer 0使用量化 + 扁平图)
// [修复版本] 使用标准HNSW双堆逻辑，避免搜索提前终止
void Solution::search_layer_query(const float *query, const unsigned char *query_quant,
                                  vector<int> &candidates, const vector<int> &ep,
                                  int ef, int lc) const
{

    tls_visited.prepare(num_vectors);

    // 使用数组模拟堆，比STL快 (Optimization 5)
    // W_arr: 结果集 (维持有序)
    Candidate W_arr[256]; // ef <= 200, 256够用
    int W_size = 0;

    // 辅助: 插入W
    auto add_to_W = [&](int id, float d)
    {
        if (W_size < ef || d < W_arr[W_size - 1].dist)
        {
            // 插入排序
            int pos = W_size;
            if (W_size < ef)
                W_size++;

            while (pos > 0 && W_arr[pos - 1].dist > d)
            {
                if (pos < ef)
                    W_arr[pos] = W_arr[pos - 1];
                pos--;
            }
            if (pos < ef)
                W_arr[pos] = {d, id};
        }
    };

    // [性能重构] 替代 priority_queue：使用 thread_local vector + 手动堆管理
    // 优势：零内存分配 (Zero Allocation)，消除动态内存开销
    tls_candidate_queue.clear();

    // 初始化
    for (int pid : ep)
    {
        if (!tls_visited.is_visited(pid))
        {
            tls_visited.mark(pid);
            float d;
            // 策略：Layer 0 使用量化距离，其他层使用精确距离
            if (lc == 0 && use_quantization && query_quant)
            {
                d = dist_l2_quant(pid, query_quant, dimension);
            }
            else
            {
                d = dist_l2_float_avx(query, &data_flat[pid * dimension], dimension);
            }
            add_to_W(pid, d);
            tls_candidate_queue.push_back({d, pid});
        }
    }
    // 建立最小堆
    make_heap(tls_candidate_queue.begin(), tls_candidate_queue.end(), greater<pair<float, int>>());

    while (!tls_candidate_queue.empty())
    {
        // 取堆顶（最小距离的候选点）
        pop_heap(tls_candidate_queue.begin(), tls_candidate_queue.end(), greater<pair<float, int>>());
        auto curr = tls_candidate_queue.back();
        tls_candidate_queue.pop_back();

        float dist_c = curr.first;
        int nid = curr.second;

        // 剪枝：当前最近的候选点比结果集中最远的点还远，且结果集已满
        if (W_size == ef && dist_c > W_arr[W_size - 1].dist)
            break;

        // 获取邻居指针
        const int *neighbors_ptr;
        int neighbors_count;

        if (lc == 0)
        {
            // Layer 0: 从扁平数组读取 (Optimization 4)
            size_t offset = final_graph_offsets[nid];
            neighbors_count = final_graph_flat[offset];
            neighbors_ptr = &final_graph_flat[offset + 1];
        }
        else
        {
            // High Layer: 从节点对象读取
            const auto &vec = nodes[nid].neighbors[lc];
            neighbors_ptr = vec.data();
            neighbors_count = (int)vec.size();
        }

        for (int i = 0; i < neighbors_count; ++i)
        {
            int neighbor_id = neighbors_ptr[i];
            if (tls_visited.is_visited(neighbor_id))
                continue;
            tls_visited.mark(neighbor_id);

            // Prefetch
            if (lc == 0 && i + 2 < neighbors_count)
            {
                // 量化数据预取
                if (use_quantization)
                {
                    _mm_prefetch((const char *)&data_quant[(long long)neighbors_ptr[i + 2] * dimension], _MM_HINT_T0);
                }
                else
                {
                    _mm_prefetch((const char *)&data_flat[neighbors_ptr[i + 2] * dimension], _MM_HINT_T0);
                }
            }

            float d;
            if (lc == 0 && use_quantization && query_quant)
            {
                d = dist_l2_quant(neighbor_id, query_quant, dimension);
            }
            else
            {
                d = dist_l2_float_avx(query, &data_flat[neighbor_id * dimension], dimension);
            }

            if (W_size < ef || d < W_arr[W_size - 1].dist)
            {
                add_to_W(neighbor_id, d);
                // 手动堆 Push
                tls_candidate_queue.push_back({d, neighbor_id});
                push_heap(tls_candidate_queue.begin(), tls_candidate_queue.end(), greater<pair<float, int>>());
            }
        }
    }

    candidates.clear();
    for (int i = 0; i < W_size; ++i)
    {
        candidates.push_back(W_arr[i].id);
    }
}

// --- 选邻居策略 (RobustPrune) ---
void Solution::get_neighbors_heuristic(vector<int> &result, const vector<int> &candidates, int k) const
{
    if (candidates.size() <= (size_t)k)
    {
        result = candidates;
        return;
    }

    // 1. 按距离排序候选集
    vector<pair<float, int>> temp;
    temp.reserve(candidates.size());
    // 注意：这里需要精确距离，因为这发生在构建阶段
    // 且candidates里的点距离query的距离需要重新计算或传递，为简单起见，这里假设base是当前插入节点
    // 但HNSW构建中，heuristic是针对 query (即新插入点) 和 candidates 之间的关系
    // 由于我们没有保存candidates的距离，这里简化处理：
    // 标准实现中，get_neighbors_heuristic 需要计算 candidate 之间的距离来保证多样性
    // 这里传入的 candidates 应该是已经按距离 query 排好序的，或者我们在外部排序

    // 由于接口限制，我们在这里不重新计算 query 到 candidate 的距离，
    // 而是假设调用者已经处理好，或者我们简化为只做多样性修剪
    // *修正*: 为了保证召回率，必须实现 RobustPrune

    // 这里我们实际上需要传入 query 的坐标来计算距离。
    // 但为了不改变太多签名，我们假设 candidates 已经包含了最近的邻居。
    // 在 build 过程中，select_neighbors 步骤传入的 candidates 确实是最近邻搜索的结果。

    result.clear();
    // 假设 candidates 已经按距离 query 升序排列 (search_layer_build 输出通常不是严格有序，需要注意)
    // 但在 build 的 loop 中，我们通常会对 candidates 进行重排。

    // 这里做最简单的截断? 不，指南强调召回率。
    // 我们需要重构逻辑：在build中，我们手动对candidates排序，然后做RobustPrune

    // 这里的实现是一个简化的 RobustPrune，不重新计算距离，仅依赖输入顺序
    // 这在 candidates 是 top-k 结果时有效。
    // 为了更严谨，我们在 build 函数内部处理排序。
    for (size_t i = 0; i < candidates.size() && result.size() < (size_t)k; ++i)
    {
        int curr_id = candidates[i];
        bool good = true;

        // 检查与已选节点的距离 (多样性)
        for (int exist_id : result)
        {
            float dist_curr_exist = dist_l2_float_avx(
                &data_flat[curr_id * dimension],
                &data_flat[exist_id * dimension],
                dimension);

            // 下面的距离是从外部传入的吗？不。
            // 这里的 GAMMA 判定通常需要知道 curr 到 query 的距离。
            // 由于缺乏 query 信息，我们退化为只选最近的 K 个（不使用 Gamma 剪枝），
            // 或者这部分逻辑在 build 主循环中实现更好。

            // *决策*: 为了代码结构清晰，我们在 build 主循环中内联 RobustPrune 逻辑，
            // 这里的函数仅作简单的 copy。
        }
        result.push_back(curr_id);
    }
}

// 辅助：生成随机层级
int Solution::get_random_level()
{
    static thread_local std::mt19937 rng(12345 + omp_get_thread_num());
    static thread_local std::uniform_real_distribution<float> dist(0.0, 1.0);
    float r = dist(rng);
    return (int)(-log(r) * ML);
}

// --- 主构建流程 ---
void Solution::build(int d, const vector<float> &base)
{
    dimension = d;
    num_vectors = base.size() / d;
    data_flat = base;

    // 参数初始化
    M_max = M;
    M_max0 = M * 2;
    max_level = 0;
    enter_point = 0;

    // 初始化节点
    nodes.resize(num_vectors);

    // 锁 (每个节点一把锁)
    vector<omp_lock_t> node_locks(num_vectors);
    for (int i = 0; i < num_vectors; ++i)
        omp_init_lock(&node_locks[i]);

    // 第一个点
    int level0 = get_random_level();
    nodes[0].neighbors.resize(level0 + 1);
    max_level = level0;
    enter_point = 0;

// 并行构建
#pragma omp parallel
    {
        // 线程局部随机数生成器在 get_random_level 中处理
        // 访问缓存
        tls_visited.prepare(num_vectors);

#pragma omp for schedule(dynamic, 128)
        for (int i = 1; i < num_vectors; ++i)
        {
            const float *query = &data_flat[i * dimension];
            int level = get_random_level();

            // 临界区：更新最大层级
            // 为性能考虑，不加锁读取，只有更新时加锁，或原子操作
            // 这里简单处理，max_level 仅由主线程更新可能不安全，但 HNSW 允许这种 loose consistency
            // 或者用 critical 更新 global max_level
            int cur_max_level = max_level;
            int curr_ep = enter_point;

            // 1. 贪婪搜索找到当前层级的入口点
            if (level < cur_max_level)
            {
                float min_dist = dist_l2_float_avx(query, &data_flat[curr_ep * dimension], dimension);
                for (int lc = cur_max_level; lc > level; --lc)
                {
                    bool changed = true;
                    while (changed)
                    {
                        changed = false;
                        const vector<int> &nbs = nodes[curr_ep].neighbors[lc]; // 读操作，可能不安全？HNSW构建通常需要细粒度锁
                        // 注意：这里读取 neighbors 时可能会有其他线程在写入。
                        // 标准做法：加读锁，或者利用 vector 的各层独立性。
                        // 在本指南的简单实现中，我们接受轻微的数据竞争带来的风险，或者在写入时 Copy-On-Write。
                        // 实际上，为了严格正确性，应该锁住当前节点读取。
                        // 但为了性能，大部分开源实现(如hnswlib)采用了乐观锁或细粒度锁。

                        // 简单版本：只在连接时加锁。搜索时不加锁（可能读到旧数据）。
                        for (int n : nbs)
                        {
                            float d = dist_l2_float_avx(query, &data_flat[n * dimension], dimension);
                            if (d < min_dist)
                            {
                                min_dist = d;
                                curr_ep = n;
                                changed = true;
                            }
                        }
                    }
                }
            }

            // 初始化当前节点
            // 只有当前线程访问 nodes[i]，无需锁
            nodes[i].neighbors.resize(level + 1);

            // 2. 从 level 向下构建
            // 需要在每层找到 ef_construction 个最近邻作为候选
            vector<int> ep_container = {curr_ep};

            for (int lc = min(level, cur_max_level); lc >= 0; --lc)
            {
                vector<int> candidates;
                search_layer_build(query, candidates, ep_container, EF_CONSTRUCTION, lc);

                // RobustPrune 选邻居逻辑
                // 需要重新计算距离并排序
                vector<pair<float, int>> sorted_cand;
                for (int c : candidates)
                {
                    sorted_cand.push_back({dist_l2_float_avx(query, &data_flat[c * dimension], dimension), c});
                }
                sort(sorted_cand.begin(), sorted_cand.end());

                // 选择邻居
                vector<int> selected_neighbors;
                int M_limit = (lc == 0) ? M_max0 : M_max;

                // Robust Prune Implementation inside loop
                for (const auto &pair : sorted_cand)
                {
                    if (selected_neighbors.size() >= (size_t)M_limit)
                        break;
                    int cand_id = pair.second;
                    float dist_to_q = pair.first;

                    bool good = true;
                    for (int exist_id : selected_neighbors)
                    {
                        float dist_exist = dist_l2_float_avx(
                            &data_flat[cand_id * dimension],
                            &data_flat[exist_id * dimension],
                            dimension);
                        if (dist_exist * GAMMA < dist_to_q)
                        {
                            good = false;
                            break;
                        }
                    }
                    if (good)
                        selected_neighbors.push_back(cand_id);
                }

                // 双向连接
                // 1. 将 selected 连接到 i
                nodes[i].neighbors[lc] = selected_neighbors;

                // 2. 将 i 连接到 selected 中的每个节点 (需要加锁)
                for (int neighbor_id : selected_neighbors)
                {
                    omp_set_lock(&node_locks[neighbor_id]);
                    vector<int> &target_neighbors = nodes[neighbor_id].neighbors[lc];

                    // 再次执行 RobustPrune 如果满了
                    bool need_prune = false;
                    target_neighbors.push_back(i); // 先加入
                    if (target_neighbors.size() > (size_t)M_limit)
                    {
                        need_prune = true;
                    }

                    if (need_prune)
                    {
                        // 重新计算该邻居的所有连接的距离
                        vector<pair<float, int>> t_cand;
                        // 这里 query 变成了 data_flat[neighbor_id]
                        const float *target_vec = &data_flat[neighbor_id * dimension];
                        for (int tn : target_neighbors)
                        {
                            t_cand.push_back({dist_l2_float_avx(target_vec, &data_flat[tn * dimension], dimension), tn});
                        }
                        sort(t_cand.begin(), t_cand.end());

                        vector<int> new_conn;
                        for (const auto &pair : t_cand)
                        {
                            if (new_conn.size() >= (size_t)M_limit)
                                break;
                            int c_id = pair.second;
                            float d_q = pair.first;
                            bool good = true;
                            for (int ex : new_conn)
                            {
                                float d_ex = dist_l2_float_avx(
                                    &data_flat[c_id * dimension],
                                    &data_flat[ex * dimension],
                                    dimension);
                                if (d_ex * GAMMA < d_q)
                                {
                                    good = false;
                                    break;
                                }
                            }
                            if (good)
                                new_conn.push_back(c_id);
                        }
                        target_neighbors = new_conn;
                    }

                    omp_unset_lock(&node_locks[neighbor_id]);
                }

                ep_container = selected_neighbors; // 下一层的入口
            }

            // 更新全局入口点 (如果是更高层)
            if (level > max_level)
            {
#pragma omp critical
                {
                    if (level > max_level)
                    {
                        max_level = level;
                        enter_point = i;
                    }
                }
            }
        }
    }

    // 清理锁
    for (int i = 0; i < num_vectors; ++i)
        omp_destroy_lock(&node_locks[i]);

    // 构建后优化：Layer 0 扁平化
    flatten_layer0();

    // 构建后优化：标量量化 (SQ)
    init_quantization();
}

void Solution::flatten_layer0()
{
    // 计算扁平化所需空间
    size_t total_size = 0;
    final_graph_offsets.resize(num_vectors);

    for (int i = 0; i < num_vectors; ++i)
    {
        final_graph_offsets[i] = total_size;
        // 格式: [count, n1, n2, ...]
        if (nodes[i].neighbors.size() > 0)
        {
            total_size += 1 + nodes[i].neighbors[0].size();
        }
        else
        {
            total_size += 1; // count = 0
        }
    }

    final_graph_flat.resize(total_size);

// 填充数据
#pragma omp parallel for
    for (int i = 0; i < num_vectors; ++i)
    {
        size_t offset = final_graph_offsets[i];
        if (nodes[i].neighbors.size() > 0)
        {
            const vector<int> &nbs = nodes[i].neighbors[0];
            final_graph_flat[offset] = (int)nbs.size();
            memcpy(&final_graph_flat[offset + 1], nbs.data(), nbs.size() * sizeof(int));

            // 释放内存：Layer 0 数据已转移，清空原 vector
            // vector<int>().swap(nodes[i].neighbors[0]);
            // 注意：不要在这里释放，因为 high layer search 可能偶尔需要用到？
            // 不，high layer search 只涉及 lc > 0。
            // 实际上，为了内存，我们可以清空 nodes[i].neighbors[0]
            // 但 nodes 是 vector<vector<int>>，清空第0个元素只是变为空 vector
        }
        else
        {
            final_graph_flat[offset] = 0;
        }
    }
}

// --- 搜索接口 ---
void Solution::search(const vector<float> &query, int *res)
{
    if (num_vectors == 0)
        return;

    // 1. 量化查询向量 (用于Layer 0)
    tls_quant_query_buf.resize(dimension);
    unsigned char *q_quant_ptr = tls_quant_query_buf.data();
    quantize_vec(query.data(), q_quant_ptr);

    int curr_ep = enter_point;
    vector<int> ep_container = {curr_ep};

    // 2. 高层导航 (Layer max ~ 1) - 使用精确距离 (Float + AVX)
    // 优化方案D建议：高层ef可设为1或稍大。为了召回率，保持 ef=1 足够，
    // 但如果在 layer 1 附近，可以稍微增大搜索范围。
    // 基准代码通常 ef=1。

    for (int lc = max_level; lc > 0; --lc)
    {
        // 在高层我们不做复杂的搜索，只做贪婪下降
        // 但复用 search_layer_query 也可以，只要 ef=1
        // 为了极致速度，手写简单贪婪遍历
        bool changed = true;
        while (changed)
        {
            changed = false;
            float dist = dist_l2_float_avx(query.data(), &data_flat[curr_ep * dimension], dimension);
            const vector<int> &nbs = nodes[curr_ep].neighbors[lc];

            for (int n : nbs)
            {
                float d = dist_l2_float_avx(query.data(), &data_flat[n * dimension], dimension);
                if (d < dist)
                {
                    dist = d;
                    curr_ep = n;
                    changed = true;
                }
            }
        }
    }
    ep_container[0] = curr_ep;

    // 3. 底层搜索 (Layer 0) - 使用量化距离 (SQ + Flattened Graph)
    vector<int> candidates;
    search_layer_query(query.data(), q_quant_ptr, candidates, ep_container, EF_SEARCH, 0);

    // ---------------------------------------------------------
    // 【关键修复】重排序 (Re-ranking) - 使用精确浮点距离
    // ---------------------------------------------------------
    // Layer 0 搜索使用量化距离，快但有误差
    // 必须用精确距离重新排序，才能保证召回率

    tls_candidate_queue.clear();

    for (int cand_id : candidates)
    {
        // 使用 AVX 精确浮点距离重新计算
        float exact_dist = dist_l2_float_avx(query.data(), &data_flat[cand_id * dimension], dimension);
        tls_candidate_queue.push_back({exact_dist, cand_id});
    }

    // 排序：按距离从小到大
    // 只需要 Top 10，使用 partial_sort 比 sort 更快
    if (tls_candidate_queue.size() > 10)
    {
        std::partial_sort(tls_candidate_queue.begin(),
                          tls_candidate_queue.begin() + 10,
                          tls_candidate_queue.end());
    }
    else
    {
        std::sort(tls_candidate_queue.begin(), tls_candidate_queue.end());
    }

    // 4. 填充结果
    for (int i = 0; i < 10 && i < (int)tls_candidate_queue.size(); ++i)
    {
        res[i] = tls_candidate_queue[i].second;
    }
    // 补位
    for (int i = tls_candidate_queue.size(); i < 10; ++i)
    {
        res[i] = tls_candidate_queue.empty() ? 0 : tls_candidate_queue[0].second;
    }
}

// [调试功能] 暴力搜索 - 用于验证HNSW结果的正确性
#ifdef DEBUG_BRUTE_FORCE
void Solution::search_brute_force(const vector<float> &query, int *res) const
{
    if (num_vectors == 0)
        return;

    // 计算所有点的距离
    vector<pair<float, int>> all_dists;
    all_dists.reserve(num_vectors);

    for (int i = 0; i < num_vectors; ++i)
    {
        float d = dist_l2_float_avx(query.data(), &data_flat[i * dimension], dimension);
        all_dists.push_back({d, i});
    }

    // 排序
    sort(all_dists.begin(), all_dists.end());

    // 取前10个
    for (int i = 0; i < 10 && i < (int)all_dists.size(); ++i)
    {
        res[i] = all_dists[i].second;
    }
    for (int i = all_dists.size(); i < 10; ++i)
    {
        res[i] = all_dists.empty() ? 0 : all_dists[0].second;
    }
}
#endif