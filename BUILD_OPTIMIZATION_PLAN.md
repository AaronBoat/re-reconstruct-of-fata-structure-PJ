# GLOVE 构建时间超限问题 - 优化方案

**问题**: 构建阶段超时（Time limit exceeded）  
**数据集**: GLOVE 1.19M × 100维  
**当前参数**: M=40, EF_CONSTRUCTION=300  
**日期**: 2026-01-04

---

## 一、问题分析

### 1.1 构建时间复杂度

HNSW 构建时间主要由以下因素决定：

```
总时间 ≈ N × (层数搜索 + 边选择 + 双向连接)
       ≈ N × (EF_CONSTRUCTION × 距离计算 + M × RobustPrune)
```

**当前配置瓶颈**：
- `EF_CONSTRUCTION = 300` → 每次插入需要维护 300 个候选点
- `M = 40` → 每个节点最多 80 条边（Layer 0）
- `RobustPrune` → 每次选边需要 O(M²) 距离计算

对于 1.19M 数据：
- 理论距离计算次数: `1.19M × 300 × 平均层数 ≈ 数亿次`
- 锁竞争: 双向连接时频繁加锁
- 内存分配: 大量 `vector` 动态扩展

---

## 二、优化思路（按优先级排序）

### 🔴 **优先级1: 降低 EF_CONSTRUCTION（影响最大）**

**当前**: `EF_CONSTRUCTION = 300`  
**建议**: `EF_CONSTRUCTION = 150-200`

**理由**:
- 构建时间与 `ef_c` 近似线性关系
- `ef_c=150` 可减少 50% 的候选点搜索时间
- 召回率影响: 通常下降 0.2-0.5%（仍可满足 98% 要求）

**收益**: 
- 构建时间 **减少 30-40%**
- 召回率可能略降但通常仍 > 98%

**风险**: 需要实测验证召回率

---

### 🔴 **优先级2: 降低 M（边数）**

**当前**: `M = 40`（Layer 0 有 80 条边）  
**建议**: `M = 24-32`

**理由**:
- 每个节点的边数直接影响：
  - RobustPrune 复杂度: O(M²)
  - 双向连接时的 prune 开销
  - 内存占用和缓存效率
- GLOVE 100维，相对低维，不需要太多边

**收益**:
- RobustPrune 时间减少 30-50%
- 双向连接锁竞争减少
- 构建时间 **减少 20-30%**

**风险**: 
- 搜索时间可能略增（因为边少了）
- 召回率可能略降（但影响不大）

---

### 🟡 **优先级3: 简化 RobustPrune（推荐稳妥方案）**

**当前**: 完整 RobustPrune（带 GAMMA 判定）  
**建议**: 简化为 Simple Heuristic

**方案A - 快速 Heuristic（推荐）**:
```cpp
// 简化版：只保留距离最近的 M 个点
sort(sorted_cand.begin(), sorted_cand.end());
selected_neighbors.assign(
    sorted_cand.begin(), 
    sorted_cand.begin() + min(M_limit, (int)sorted_cand.size())
);
```

**方案B - 保留部分多样性（折中）**:
```cpp
// 只对前 2*M 个候选点做 RobustPrune
int prune_limit = min(2*M_limit, (int)sorted_cand.size());
for (int i = 0; i < prune_limit && selected.size() < M_limit; ++i) {
    // RobustPrune logic
}
```

**收益**:
- 方案A: 构建时间 **减少 15-25%**
- 方案B: 构建时间 **减少 10-15%**

**风险**:
- 方案A: 召回率可能下降 0.3-1%
- 方案B: 影响较小

---

### 🟡 **优先级4: 优化双向连接的 Prune**

**当前**: 每次反向连接都完整 RobustPrune  
**建议**: 延迟 Prune 或批量 Prune

**方案**:
```cpp
// 允许暂时超过 M_limit，每隔 N 个节点批量 prune
target_neighbors.push_back(i);
if (target_neighbors.size() > M_limit * 1.2) {
    // 执行 prune
}
```

**收益**: 
- 减少锁持有时间
- 构建时间 **减少 5-10%**

**风险**: 内存占用略增

---

### 🟢 **优先级5: 减少量化开销（可选）**

**当前**: 构建后全量量化  
**建议**: 仅在必要时量化，或降低量化精度

**方案**:
- 跳过量化，直接使用浮点搜索
- 或者异步量化（不计入构建时间）

**收益**: 构建时间 **减少 2-5%**  
**风险**: 搜索时间可能增加

---

### 🟢 **优先级6: 并行调度优化**

**当前**: `#pragma omp for schedule(dynamic, 128)`  
**建议**: 调整调度策略

**选项**:
- `schedule(static)`: 适合均匀负载
- `schedule(guided)`: 自适应分块
- 增大 chunk size: `schedule(dynamic, 256)`

**收益**: 构建时间 **减少 3-8%**  
**风险**: 需要实测

---

## 三、推荐组合方案

### 📦 **方案A - 激进优化（最快）**

```cpp
M = 24
EF_CONSTRUCTION = 150
// + 简化 RobustPrune（只取最近的 M 个）
// + 延迟双向 Prune
```

**预期效果**:
- 构建时间: **减少 50-60%**（目标 < 2000秒）
- 召回率: 可能降至 97.5-98.5%
- 风险: ⚠️ 需要验证召回率是否达标

---

### 📦 **方案B - 稳妥优化（推荐）** ⭐

```cpp
M = 32
EF_CONSTRUCTION = 200
// + 部分简化 RobustPrune
// + 保留双向完整 Prune
```

**预期效果**:
- 构建时间: **减少 35-45%**
- 召回率: 应保持 > 98%
- 风险: ✅ 低风险

---

### 📦 **方案C - 保守优化（最稳）**

```cpp
M = 36
EF_CONSTRUCTION = 250
// + 仅优化并行调度
```

**预期效果**:
- 构建时间: **减少 15-25%**
- 召回率: 几乎无影响
- 风险: ✅ 极低风险

---

## 四、实施步骤

### Step 1: 快速验证参数

```powershell
# 修改 MySolution.cpp 中的常量
# static const int M = 32;
# static const int EF_CONSTRUCTION = 200;

# 编译测试
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
    test_solution.cpp MySolution.cpp -o test_solution.exe

# 运行测试（记录时间）
$env:OMP_NUM_THREADS=8
Measure-Command { .\test_solution.exe ..\data_o\data_o\glove } | Select-Object TotalSeconds
```

### Step 2: 对比召回率

```powershell
# 提取召回率
.\test_solution.exe ..\data_o\data_o\glove 2>&1 | Select-String "Recall@10"
```

### Step 3: 如果召回率仍不足

- 微调 `EF_CONSTRUCTION` 向上（+10）
- 或增加 `M`（+2）
- 或恢复完整 RobustPrune

### Step 4: 如果时间仍超

- 进一步降低 `EF_CONSTRUCTION`（-20）
- 或降低 `M`（-4）
- 或简化 RobustPrune

---

## 五、参数调优速查表

| M  | EF_C | 构建时间(预估) | 召回率(预估) | 推荐场景 |
|----|------|--------------|-------------|---------|
| 24 | 150  | 300-400s     | 97.5-98.2% | 激进优化 |
| 28 | 180  | 350-450s     | 98.0-98.5% | 平衡方案 |
| 32 | 200  | 400-500s     | 98.2-98.8% | **推荐** ⭐ |
| 36 | 250  | 500-600s     | 98.5-99.0% | 保守方案 |
| 40 | 300  | 700-900s     | 98.8-99.2% | 当前配置（超时） |

**注**: 实际效果取决于硬件和并行度

---

## 六、调试检查清单

### 构建时间分析

```cpp
// 在 build() 函数中添加计时
auto t1 = std::chrono::high_resolution_clock::now();
// ... 构建代码 ...
auto t2 = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
std::cout << "Build main loop: " << duration << " ms" << std::endl;

// 量化时间
auto t3 = std::chrono::high_resolution_clock::now();
init_quantization();
auto t4 = std::chrono::high_resolution_clock::now();
std::cout << "Quantization: " << 
    std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() << " ms" << std::endl;
```

### 常见问题排查

✅ **检查项**:
- [ ] 是否启用了 `-O3` 优化？
- [ ] 是否启用了 `-fopenmp`？
- [ ] 线程数是否设置正确（`OMP_NUM_THREADS=8`）？
- [ ] 是否有大量锁竞争？（减少 M 可缓解）
- [ ] 是否有频繁内存分配？（预分配 vector）

---

## 七、紧急简化方案（如果上述都不够）

### 🚨 **终极方案 - 单线程 + 极简参数**

```cpp
M = 16
EF_CONSTRUCTION = 100
// 禁用 RobustPrune，直接取最近的 M 个
// 禁用双向 Prune，允许度数超限
// 禁用量化
```

**预期**: 
- 构建时间: < 300s
- 召回率: 95-97%（不满足要求，但可以看到底线）

**用途**: 仅作为调试参考，不可提交

---

## 八、总结

**立即尝试的方案（无需大改代码）**:

1. **修改参数** → `M=32, EF_CONSTRUCTION=200`
2. **测试** → 观察构建时间和召回率
3. **微调** → 根据结果调整 ±10-20

**预期结果**:
- 构建时间从 ~700s 降至 **400-500s**
- 召回率保持 **98.2-98.5%**

**下一步**（如果参数调整不够）:
- 简化 RobustPrune（代码改动量小）
- 优化双向连接（代码改动量中等）

**最后手段**（大改）:
- 实现增量构建
- 使用近似距离
- 更换图构建策略

---

**建议**: 先尝试 **方案B（M=32, EF_C=200）**，这是最稳妥的优化方案。
