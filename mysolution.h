#ifndef MYSOLUTION_H
#define MYSOLUTION_H

#include <vector>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <cstring>
#include <queue>      // priority_queue 支持
#include <functional> // greater<T> 支持
#include <utility>    // pair 支持

using namespace std;

class Solution
{
public:
    // 接口约束
    void build(int d, const vector<float> &base);
    void search(const vector<float> &query, int *res);

private:
    // --- 数据存储 ---
    int dimension;
    int num_vectors;

    // 原始向量 (用于构建和高层搜索)
    vector<float> data_flat;

    // 量化相关 (用于Layer 0快速搜索)
    vector<unsigned char> data_quant;
    float global_min;
    float global_scale_inv;
    bool use_quantization;

    // --- HNSW 图结构 ---
    struct Node
    {
        // [level][neighbor_index]
        // 注意：构建完成后，Level 0 将被移动到 final_graph_flat 优化访问
        vector<vector<int>> neighbors;
    };
    vector<Node> nodes;

    // 优化后的 Layer 0 (扁平化存储: [size, n1, n2, ..., size, n1, ...])
    vector<int> final_graph_flat;
    vector<size_t> final_graph_offsets; // 快速定位 flattened graph

    int max_level;
    int enter_point;
    int M_max;
    int M_max0;

    // --- 内部辅助方法 ---

    // 距离计算
    float dist_l2_float_avx(const float *a, const float *b, int d) const;
    float dist_l2_quant(int id_a, const unsigned char *b_quant, int d) const;

    // 量化工具
    void init_quantization();
    void quantize_vec(const float *src, unsigned char *dst) const;

    // 图操作
    int get_random_level();
    void get_neighbors_heuristic(vector<int> &result, const vector<int> &candidates, int k) const;

    // 核心搜索逻辑 (分为构建用和查询用)

    // 1. 通用/构建搜索 (精确距离，动态图)
    void search_layer_build(const float *query, std::vector<int> &candidates,
                            const std::vector<int> &ep, int ef, int lc) const;

    // 2. 最终查询搜索 (混合精度，Layer 0扁平化)
    void search_layer_query(const float *query, const unsigned char *query_quant,
                            std::vector<int> &candidates, const std::vector<int> &ep,
                            int ef, int lc) const;

    // 扁平化 Layer 0
    void flatten_layer0();

// [调试用] 暴力搜索 (Golden Standard)
#ifdef DEBUG_BRUTE_FORCE
    void search_brute_force(const vector<float> &query, int *res) const;
#endif
};

#endif // MYSOLUTION_H