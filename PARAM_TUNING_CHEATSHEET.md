# 参数调优速查表

## 当前状态
- **基准参数**: M=30, EF_CONSTRUCTION=200, GAMMA=1.0
- **基准性能**: Recall@10 = 95.9%
- **目标**: Recall@10 ≥ 98%

## 推荐测试顺序

### 优先级 1️⃣ - 最有希望（预计召回率 98%+）

```cpp
// 配置 A: 增加图密度和构建质量
static const int M = 40;
static const int EF_CONSTRUCTION = 300;
static const float GAMMA = 1.0f;
// 预期: R@10 ≈ 98-99%, Build ≈ 600-800s
```

```cpp
// 配置 B: 更激进的参数
static const int M = 48;
static const int EF_CONSTRUCTION = 350;
static const float GAMMA = 1.0f;
// 预期: R@10 ≈ 99%+, Build ≈ 900-1200s
```

### 优先级 2️⃣ - 稳健方案（预计召回率 97-98%）

```cpp
// 配置 C: 仅增加M
static const int M = 36;
static const int EF_CONSTRUCTION = 200;
static const float GAMMA = 1.0f;
// 预期: R@10 ≈ 96.5-97.5%, Build ≈ 500s
```

```cpp
// 配置 D: 仅增加EF_CONSTRUCTION
static const int M = 30;
static const int EF_CONSTRUCTION = 300;
static const float GAMMA = 1.0f;
// 预期: R@10 ≈ 97-98%, Build ≈ 550s
```

### 优先级 3️⃣ - 实验方案（调整GAMMA）

```cpp
// 配置 E: 降低GAMMA增加多样性
static const int M = 36;
static const int EF_CONSTRUCTION = 250;
static const float GAMMA = 0.75f;
// 预期: R@10 ≈ 96-97% (不确定)
```

## 快速测试命令

```powershell
# 1. 修改 mysolution.cpp 第 11-15 行的参数

# 2. 编译
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp test_solution.cpp mysolution.cpp -o test_solution.exe

# 3. 测试
$env:OMP_NUM_THREADS=8
.\test_solution.exe | Select-String "Build time|Average search|Recall"
```

## 结果记录表

| 配置 | M  | EF_CONST | GAMMA | Build(s) | Search(ms) | R@1  | R@10 | 状态 |
|------|----|----------|-------|----------|------------|------|------|------|
| 基准 | 30 | 200      | 1.0   | 433.9    | 0.89       | 100% | 95.9%| ⚠️   |
| A    | 40 | 300      | 1.0   | ?        | ?          | ?    | ?    | -    |
| B    | 48 | 350      | 1.0   | ?        | ?          | ?    | ?    | -    |
| C    | 36 | 200      | 1.0   | ?        | ?          | ?    | ?    | -    |
| D    | 30 | 300      | 1.0   | ?        | ?          | ?    | ?    | -    |
| E    | 36 | 250      | 0.75  | ?        | ?          | ?    | ?    | -    |

## 参数影响分析

- **M (邻居数上限)**
  - M ↑ → 召回率 ↑, 构建时间 ↑, 搜索时间 ↑
  - 推荐范围: 30-48
  - 过大可能超时(>2000s)

- **EF_CONSTRUCTION (构建时候选集大小)**
  - EF_CONSTRUCTION ↑ → 召回率 ↑, 构建时间 ↑
  - 推荐范围: 200-400
  - 最有效的参数之一

- **GAMMA (RobustPrune多样性因子)**
  - GAMMA ↓ → 多样性 ↑, 召回率可能↑或↓
  - 推荐范围: 0.5-1.5
  - 影响最不确定

## 决策树

```
召回率 < 98%?
├─ YES
│  ├─ 构建时间 < 1000s?
│  │  ├─ YES → 测试配置 A (M=40, EF=300)
│  │  └─ NO  → 测试配置 D (M=30, EF=300)
│  └─ 构建时间 ≥ 1000s?
│     └─ 尝试配置 C (M=36, EF=200)
└─ NO → 🎉 完成！

配置 A 仍不达标?
└─ 测试配置 B (M=48, EF=350)
```

## 注意事项

1. **时间控制**: 确保构建时间 < 2000秒
2. **搜索时间**: 应保持在 < 20ms (通常不是问题)
3. **单次测试**: 约 8-15 分钟
4. **备份参数**: 每次修改前记录当前配置

## 终止条件

满足以下全部条件即可停止：
- ✅ Recall@10 ≥ 98%
- ✅ Build time < 2000s
- ✅ Average search time < 20ms

---

**当前推荐**: 立即测试配置 A (M=40, EF_CONSTRUCTION=300)，成功概率最高！
