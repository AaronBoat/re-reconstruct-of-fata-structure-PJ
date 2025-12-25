# HNSW项目测试环境完整搭建指南

**面向**: 重构者/新开发者  
**目标**: 从零开始搭建完整的测试环境  
**更新日期**: 2025-12-25

---

## 一、目录结构概览

### 推荐的项目结构

```
workspace/
├── re_reconstruct/                # 代码工作目录
│   ├── MySolution.h           # 解决方案头文件
│   ├── MySolution.cpp         # 解决方案实现
│   ├── test_solution.cpp      # 测试驱动程序（本文档提供）
│   ├── Makefile              # 可选的构建文件
│   └── *.ps1                 # PowerShell辅助脚本
│
└── data_o/                    # 数据集目录（需要下载）
    └── data_o/
        ├── glove/            # GLOVE数据集（主要评测）
        │   ├── base.txt     # 1.19M × 100维基础向量
        │   ├── query.txt    # 100个查询向量
        │   └── groundtruth.txt  # 真实最近邻
        │
        ├── sift/            # SIFT数据集（可选测试）
        │   ├── base.txt     # 1M × 128维基础向量
        │   ├── query.txt    # 100个查询向量
        │   └── groundtruth.txt
        │
        └── sift_small/      # SIFT小数据集（快速验证）
            ├── base.txt     # 10K × 128维
            ├── query.txt    # 100个查询向量
            └── groundtruth.txt
```

---

## 二、数据集准备

### 2.1 数据集说明

#### GLOVE数据集（必需）

**规模**: 1,192,514个向量 × 100维  
**用途**: 主要评测数据集，性能指标以此为准  
**文件大小**: 约500MB (base.txt)

**数据格式** (base.txt):
```
-0.079084 0.081468 -0.023201 ... (100个浮点数，空格分隔)
-0.015672 -0.053289 0.098341 ...
...
```

**查询格式** (query.txt):
```
100 100                        # 第一行：查询数量 维度（可选）
-0.034521 0.067823 -0.012345 ... (100个浮点数)
...
```

**真实结果格式** (groundtruth.txt):
```
100 100                        # 第一行：查询数量 K值（可选）
0 15678 234567 ...            # 每行100个整数，表示最近邻的向量ID
...
```

---



### 2.2 数据集获取方式

#### 方式1: 从课程平台下载（推荐）

如果你有课程账号，直接从平台下载数据包：
```
data_o.zip  或  data_o.tar.gz
```

解压后放置到正确位置：
```powershell
# PowerShell命令
Expand-Archive -Path data_o.zip -DestinationPath ../

# 或使用tar
tar -xf data_o.tar.gz -C ../
```

---

#### 方式2: 生成测试数据（如果无法获取原始数据）

如果无法获取原始数据集，可以生成模拟数据用于开发测试：

```python
# generate_test_data.py
import numpy as np
import os

def generate_glove_like_data(output_dir="glove_synthetic"):
    """生成类似GLOVE的合成数据集"""
    os.makedirs(output_dir, exist_ok=True)
    
    # 参数
    num_base = 10000      # 减小规模用于测试（原始1.19M）
    num_query = 100
    dimension = 100
    k = 100               # top-k
    
    print(f"生成合成GLOVE数据集...")
    print(f"  基础向量: {num_base} × {dimension}维")
    print(f"  查询向量: {num_query} × {dimension}维")
    
    # 1. 生成基础向量（正态分布）
    np.random.seed(42)
    base_vectors = np.random.randn(num_base, dimension).astype(np.float32)
    
    # 归一化（可选，模拟真实数据分布）
    base_vectors = base_vectors / np.linalg.norm(base_vectors, axis=1, keepdims=True)
    
    # 保存base.txt
    print("  写入 base.txt...")
    with open(os.path.join(output_dir, "base.txt"), "w") as f:
        for vec in base_vectors:
            f.write(" ".join(map(str, vec)) + "\n")
    
    # 2. 生成查询向量
    query_vectors = np.random.randn(num_query, dimension).astype(np.float32)
    query_vectors = query_vectors / np.linalg.norm(query_vectors, axis=1, keepdims=True)
    
    # 保存query.txt
    print("  写入 query.txt...")
    with open(os.path.join(output_dir, "query.txt"), "w") as f:
        f.write(f"{num_query} {dimension}\n")  # 头部元数据
        for vec in query_vectors:
            f.write(" ".join(map(str, vec)) + "\n")
    
    # 3. 计算真实最近邻（暴力搜索）
    print("  计算groundtruth（暴力搜索）...")
    groundtruth = []
    for i, query in enumerate(query_vectors):
        # 计算与所有基础向量的距离
        distances = np.sum((base_vectors - query) ** 2, axis=1)
        # 找到最近的k个
        nearest_indices = np.argsort(distances)[:k]
        groundtruth.append(nearest_indices)
        
        if (i + 1) % 10 == 0:
            print(f"    完成 {i+1}/{num_query} 个查询")
    
    # 保存groundtruth.txt
    print("  写入 groundtruth.txt...")
    with open(os.path.join(output_dir, "groundtruth.txt"), "w") as f:
        f.write(f"{num_query} {k}\n")
        for gt in groundtruth:
            f.write(" ".join(map(str, gt)) + "\n")
    
    print(f"✓ 合成数据集生成完成：{output_dir}/")
    print(f"  文件大小: base.txt ~{os.path.getsize(os.path.join(output_dir, 'base.txt'))//1024}KB")

if __name__ == "__main__":
    # 生成小规模测试数据
    generate_glove_like_data("glove_small")
    
    # 如果需要大规模数据（警告：非常慢）
    # generate_glove_like_data("glove_large", num_base=100000)
```

**运行生成脚本**:
```powershell
python generate_test_data.py
```

**注意**: 
- 合成数据只能用于功能验证，**不能**作为性能评测标准
- 真实数据集的分布特性影响HNSW性能
- 提交时必须使用原始GLOVE数据集测试

---

### 2.3 验证数据集完整性

```powershell
# 检查数据集脚本
function Test-Dataset {
    param([string]$DatasetDir)
    
    $files = @("base.txt", "query.txt", "groundtruth.txt")
    $allExist = $true
    
    Write-Host "`n检查数据集: $DatasetDir" -ForegroundColor Cyan
    
    foreach ($file in $files) {
        $path = Join-Path $DatasetDir $file
        if (Test-Path $path) {
            $size = (Get-Item $path).Length / 1MB
            Write-Host "  ✓ $file - $([math]::Round($size, 2)) MB" -ForegroundColor Green
        } else {
            Write-Host "  ✗ $file - 缺失" -ForegroundColor Red
            $allExist = $false
        }
    }
    
    if ($allExist) {
        Write-Host "`n✓ 数据集完整" -ForegroundColor Green
        
        # 验证文件格式
        $basePath = Join-Path $DatasetDir "base.txt"
        $firstLine = Get-Content $basePath -First 1
        $dimension = ($firstLine -split " ").Count
        
        Write-Host "  检测到维度: $dimension" -ForegroundColor Cyan
        
        $numLines = (Get-Content $basePath | Measure-Object -Line).Lines
        Write-Host "  基础向量数: $numLines" -ForegroundColor Cyan
    } else {
        Write-Host "`n✗ 数据集不完整，请检查" -ForegroundColor Red
    }
}

# 使用示例
Test-Dataset "..\data_o\data_o\glove"
Test-Dataset "..\data_o\data_o\sift"
Test-Dataset "..\data_o\data_o\sift_small"
```

**预期输出** (GLOVE):
```
检查数据集: ..\data_o\data_o\glove
  ✓ base.txt - 477.23 MB
  ✓ query.txt - 0.04 MB
  ✓ groundtruth.txt - 0.04 MB

✓ 数据集完整
  检测到维度: 100
  基础向量数: 1192514
```

---

## 三、测试驱动程序说明

### 3.1 test_solution.cpp 功能说明

test_solution.cpp是官方提供的测试框架，负责：

1. **加载数据集**: 读取base.txt, query.txt, groundtruth.txt
2. **构建索引**: 调用 `solution.build(dimension, base_vectors)`
3. **执行搜索**: 对每个查询调用 `solution.search(query, results)`
4. **计算指标**: 构建时间、搜索时间、召回率@1、召回率@10
5. **缓存支持**: 可保存/加载已构建的图结构

---

### 3.2 test_solution.cpp 完整源代码

将以下代码保存为 `test_solution.cpp`:

```cpp
#include "MySolution.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <string>
#include <set>

using namespace std;

// Load base vectors from file (line-by-line format)
vector<float> load_base_vectors(const string &filename, int &dimension, int &num_vectors)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Failed to open file: " << filename << endl;
        return vector<float>();
    }

    string line;
    vector<float> vectors;
    num_vectors = 0;
    dimension = 0;

    cout << "Loading vectors..." << flush;
    int progress_counter = 0;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        istringstream iss(line);
        vector<float> vec;
        float val;

        while (iss >> val)
        {
            vec.push_back(val);
        }

        if (vec.empty())
            continue;

        // Determine dimension from first vector
        if (dimension == 0)
        {
            dimension = vec.size();
            cout << " (dimension: " << dimension << ")" << endl;
        }

        // Add vector data
        for (float v : vec)
        {
            vectors.push_back(v);
        }
        num_vectors++;

        // Progress indicator
        if (++progress_counter % 100000 == 0)
        {
            cout << "  Loaded " << num_vectors << " vectors..." << endl;
        }
    }

    file.close();
    return vectors;
}

// Load query vectors from file
vector<vector<float>> load_query_vectors(const string &filename, int dimension)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Failed to open file: " << filename << endl;
        return vector<vector<float>>();
    }

    string line;
    vector<vector<float>> queries;
    bool first_line = true;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        istringstream iss(line);
        vector<float> vec;
        float val;

        while (iss >> val)
        {
            vec.push_back(val);
        }

        if (vec.empty())
            continue;

        // Skip metadata line (e.g., "100 128")
        if (first_line && vec.size() == 2 && vec[0] > 0 && vec[1] > 0 && vec[0] < 100000 && vec[1] < 1000)
        {
            first_line = false;
            continue;
        }
        first_line = false;

        // Validate dimension
        if (dimension > 0 && vec.size() != dimension)
        {
            cerr << "Query dimension mismatch! Expected " << dimension
                 << ", got " << vec.size() << " in line" << endl;
            continue;
        }

        queries.push_back(vec);
    }

    file.close();
    return queries;
}

// Load groundtruth from file
vector<vector<int>> load_groundtruth(const string &filename)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Failed to open file: " << filename << endl;
        return vector<vector<int>>();
    }

    string line;
    vector<vector<int>> groundtruth;
    bool first_line = true;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        istringstream iss(line);
        vector<int> vec;
        int val;

        while (iss >> val)
        {
            vec.push_back(val);
        }

        if (vec.empty())
            continue;

        // Skip metadata line if exists
        if (first_line && vec.size() == 2 && vec[0] > 0 && vec[1] > 0 && vec[0] < 100000 && vec[1] < 1000)
        {
            first_line = false;
            continue;
        }
        first_line = false;

        groundtruth.push_back(vec);
    }

    file.close();
    return groundtruth;
}

// Calculate recall@K
double calculate_recall(const vector<vector<int>> &results, const vector<vector<int>> &groundtruth, int k)
{
    if (results.size() != groundtruth.size())
    {
        cerr << "Results and groundtruth size mismatch!" << endl;
        return 0.0;
    }

    int total_recall = 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        set<int> gt_set;
        for (int j = 0; j < min(k, (int)groundtruth[i].size()); ++j)
        {
            gt_set.insert(groundtruth[i][j]);
        }

        int hits = 0;
        for (int j = 0; j < min(k, (int)results[i].size()); ++j)
        {
            if (gt_set.count(results[i][j]) > 0)
            {
                hits++;
            }
        }
        total_recall += hits;
    }

    return (double)total_recall / (results.size() * k);
}

int main(int argc, char *argv[])
{
    // Default to SIFT dataset
    string dataset_dir = "../data_o/data_o/sift";
    bool use_cache = false;
    bool save_cache = false;
    int custom_ef_search = -1;

    if (argc > 1)
    {
        dataset_dir = argv[1];
    }

    // Parse command line arguments
    for (int i = 2; i < argc; ++i)
    {
        string arg = argv[i];
        if (arg == "--use-cache")
        {
            use_cache = true;
        }
        else if (arg == "--save-cache")
        {
            save_cache = true;
        }
        else if (arg == "--ef-search" && i + 1 < argc)
        {
            custom_ef_search = atoi(argv[i + 1]);
            ++i;
        }
    }

    string base_file = dataset_dir + "/base.txt";
    string query_file = dataset_dir + "/query.txt";
    string groundtruth_file = dataset_dir + "/groundtruth.txt";
    string cache_file = dataset_dir + "_graph_cache.bin";

    cout << "Using dataset: " << dataset_dir << endl;

    // Try to load cached graph first
    Solution solution;
    bool loaded_from_cache = false;
    int dimension = 0, num_vectors = 0;

    if (use_cache)
    {
        cout << "Attempting to load graph from cache: " << cache_file << endl;
        auto cache_start = chrono::high_resolution_clock::now();
        if (solution.load_graph(cache_file))
        {
            auto cache_end = chrono::high_resolution_clock::now();
            auto cache_time = chrono::duration_cast<chrono::milliseconds>(cache_end - cache_start).count();
            cout << "✓ Graph loaded from cache in " << cache_time << " ms" << endl;
            loaded_from_cache = true;

            // Get dimension from a quick peek at base file for query loading
            ifstream peek(base_file);
            if (peek.is_open())
            {
                string line;
                getline(peek, line);
                istringstream iss(line);
                float val;
                while (iss >> val)
                    dimension++;
            }
        }
        else
        {
            cout << "✗ Failed to load cache, will build new graph..." << endl;
        }
    }

    if (!loaded_from_cache)
    {
        cout << "Loading base vectors..." << endl;
        vector<float> base_vectors = load_base_vectors(base_file, dimension, num_vectors);

        if (base_vectors.empty())
        {
            cerr << "Failed to load base vectors" << endl;
            return 1;
        }

        cout << "Loaded " << num_vectors << " vectors of dimension " << dimension << endl;

        // Build index
        auto build_start = chrono::high_resolution_clock::now();
        solution.build(dimension, base_vectors);
        auto build_end = chrono::high_resolution_clock::now();
        auto build_time = chrono::duration_cast<chrono::milliseconds>(build_end - build_start).count();

        cout << "\nBuild time: " << build_time << " ms" << endl;

        // Save cache if requested
        if (save_cache)
        {
            cout << "Saving graph cache to: " << cache_file << endl;
            if (solution.save_graph(cache_file))
            {
                cout << "✓ Graph cache saved successfully" << endl;
            }
            else
            {
                cout << "✗ Failed to save graph cache" << endl;
            }
        }
    }

    // Apply custom ef_search if specified
    if (custom_ef_search > 0)
    {
        cout << "Setting ef_search to " << custom_ef_search << endl;
        solution.set_ef_search(custom_ef_search);
    }

    // Load and search queries
    cout << "\nLoading query vectors..." << endl;
    vector<vector<float>> queries = load_query_vectors(query_file, dimension);

    if (queries.empty())
    {
        cerr << "Failed to load queries" << endl;
        return 1;
    }

    cout << "Loaded " << queries.size() << " query vectors" << endl;

    // Load groundtruth
    cout << "\nLoading groundtruth..." << endl;
    vector<vector<int>> groundtruth = load_groundtruth(groundtruth_file);

    if (groundtruth.empty())
    {
        cerr << "Failed to load groundtruth (continuing without recall calculation)" << endl;
    }
    else
    {
        cout << "Loaded groundtruth for " << groundtruth.size() << " queries" << endl;
    }

    // Perform searches
    cout << "\nPerforming searches..." << endl;

    // Reset distance computation counter before searching
    solution.reset_distance_computations();

    auto search_start = chrono::high_resolution_clock::now();

    vector<vector<int>> all_results;
    for (size_t i = 0; i < queries.size(); ++i)
    {
        int results[10];
        solution.search(queries[i], results);

        vector<int> result_vec(results, results + 10);
        all_results.push_back(result_vec);

        if (i < 5) // Print first 5 results
        {
            cout << "Query " << i << " results: ";
            for (int j = 0; j < 10; ++j)
            {
                cout << results[j] << " ";
            }
            cout << endl;
        }
    }

    auto search_end = chrono::high_resolution_clock::now();
    auto search_time = chrono::duration_cast<chrono::milliseconds>(search_end - search_start).count();

    cout << "\nTotal search time: " << search_time << " ms" << endl;
    cout << "Average search time: " << fixed << setprecision(2)
         << (double)search_time / queries.size() << " ms" << endl;

    // Get distance computation statistics
    long long total_distance_computations = solution.get_distance_computations();
    cout << "Total distance computations: " << total_distance_computations << endl;
    cout << "Average distance computations per query: " << fixed << setprecision(2)
         << (double)total_distance_computations / queries.size() << endl;

    // Calculate recall
    if (!groundtruth.empty() && groundtruth.size() == all_results.size())
    {
        double recall_1 = calculate_recall(all_results, groundtruth, 1);
        double recall_10 = calculate_recall(all_results, groundtruth, 10);

        cout << "\nRecall@1:  " << fixed << setprecision(4) << recall_1 << endl;
        cout << "Recall@10: " << fixed << setprecision(4) << recall_10 << endl;
    }

    return 0;
}
```

**文件说明**:
- 无需修改，直接使用
- 支持命令行参数（数据集路径、缓存等）
- 自动计算所有性能指标

---

### 3.3 命令行参数说明

```powershell
# 基本用法
.\test_solution.exe [数据集路径] [选项]

# 示例
.\test_solution.exe ..\data_o\data_o\glove              # GLOVE测试
.\test_solution.exe ..\data_o\data_o\sift --save-cache  # SIFT测试并保存缓存
.\test_solution.exe ..\data_o\data_o\glove --use-cache  # 使用缓存（跳过构建）
.\test_solution.exe ..\data_o\data_o\glove --ef-search 250  # 自定义ef_search
```

**参数列表**:

| 参数 | 说明 | 示例 |
|------|------|------|
| 位置参数1 | 数据集目录路径 | `..\data_o\data_o\glove` |
| `--save-cache` | 构建后保存图缓存 | 用于重复测试 |
| `--use-cache` | 从缓存加载图结构 | 跳过构建阶段 |
| `--ef-search N` | 自定义搜索参数 | 调优时使用 |

---

## 四、编译与运行

### 4.1 Windows + g++ (MinGW/MSYS2)

```powershell
# 1. 编译
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
    test_solution.cpp MySolution.cpp -o test_solution.exe

# 2. 设置线程数
$env:OMP_NUM_THREADS=8

# 3. 运行GLOVE测试
.\test_solution.exe ..\data_o\data_o\glove

# 4. 运行SIFT_SMALL快速验证
.\test_solution.exe ..\data_o\data_o\sift_small
```

---

### 4.2 Linux + g++

```bash
# 编译
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp \
    test_solution.cpp MySolution.cpp -o test_solution

# 运行
export OMP_NUM_THREADS=8
./test_solution ../data_o/data_o/glove
```

---

### 4.3 使用Makefile（推荐）

创建 `Makefile`:

```makefile
# Makefile for HNSW solution

CXX = g++
CXXFLAGS = -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp -Wall
TARGET = test_solution
SOURCES = test_solution.cpp MySolution.cpp
HEADERS = MySolution.h

# 默认目标
all: $(TARGET)

# 编译可执行文件
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET).exe

# 清理
clean:
	rm -f $(TARGET).exe *.o *.log

# 快速测试（SIFT_SMALL）
test-quick:
	@echo "快速测试..."
	@export OMP_NUM_THREADS=8 && ./$(TARGET).exe ../data_o/data_o/sift_small

# 完整测试（GLOVE）
test-full:
	@echo "完整测试 GLOVE..."
	@export OMP_NUM_THREADS=8 && ./$(TARGET).exe ../data_o/data_o/glove

# 性能测试（提取关键指标）
test-perf:
	@export OMP_NUM_THREADS=8 && ./$(TARGET).exe ../data_o/data_o/glove | grep -E "Build time|Average search|Recall"

.PHONY: all clean test-quick test-full test-perf
```

**使用方法**:
```powershell
make              # 编译
make test-quick   # 快速测试
make test-full    # 完整测试
make clean        # 清理
```

---

## 五、测试数据验证

### 5.1 数据加载验证

创建验证脚本 `verify_data.cpp`:

```cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: verify_data <dataset_dir>" << endl;
        return 1;
    }
    
    string dataset_dir = argv[1];
    string base_file = dataset_dir + "/base.txt";
    
    // 读取第一行
    ifstream file(base_file);
    if (!file.is_open()) {
        cerr << "Cannot open: " << base_file << endl;
        return 1;
    }
    
    string line;
    getline(file, line);
    
    // 统计维度
    istringstream iss(line);
    float val;
    int dimension = 0;
    vector<float> first_vec;
    
    while (iss >> val) {
        first_vec.push_back(val);
        dimension++;
    }
    
    cout << "Detected dimension: " << dimension << endl;
    cout << "First vector (first 10 dims): ";
    for (int i = 0; i < min(10, dimension); ++i) {
        cout << first_vec[i] << " ";
    }
    cout << endl;
    
    // 统计行数
    int count = 1;
    while (getline(file, line)) {
        if (!line.empty()) count++;
    }
    
    cout << "Total vectors: " << count << endl;
    
    file.close();
    return 0;
}
```

**编译运行**:
```powershell
g++ verify_data.cpp -o verify_data.exe
.\verify_data.exe ..\data_o\data_o\glove
```

**预期输出**:
```
Detected dimension: 100
First vector (first 10 dims): -0.079084 0.081468 -0.023201 ...
Total vectors: 1192514
```

---

## 六、常见问题排查

### 6.1 数据加载失败

**问题**: `Failed to open file: ../data_o/data_o/glove/base.txt`

**原因**: 路径不正确或数据集未下载

**解决**:
```powershell
# 检查路径
Test-Path ..\data_o\data_o\glove\base.txt

# 如果返回False，检查目录结构
Get-ChildItem ..\ -Recurse -Filter "base.txt"
```

---

### 6.2 维度不匹配

**问题**: `Query dimension mismatch! Expected 100, got 128`

**原因**: 使用了错误的数据集（如SIFT数据但代码配置为GLOVE）

**解决**: 确认数据集路径和参数配置一致

---

### 6.3 内存不足

**问题**: 程序崩溃或系统卡死

**原因**: GLOVE数据集需要约2-3GB内存

**解决**:
```powershell
# 先测试小数据集
.\test_solution.exe ..\data_o\data_o\sift_small

# 减少线程数
$env:OMP_NUM_THREADS=4
```

---

### 6.4 性能异常

**问题**: 召回率为0或搜索时间异常

**原因**: 代码实现错误或数据集损坏

**解决**:
1. 对比第六批稳定版本代码
2. 使用verify_data.exe验证数据集
3. 检查编译选项是否正确

---

## 七、完整测试清单

### 开发阶段测试

```powershell
# 1. 快速验证（10秒）
.\test_solution.exe ..\data_o\data_o\sift_small

# 2. SIFT测试（2-3分钟）
.\test_solution.exe ..\data_o\data_o\sift

# 3. GLOVE完整测试（12分钟）
.\test_solution.exe ..\data_o\data_o\glove
```

### 提交前验证

```powershell
# 完整流程（必须全部通过）
# 1. 清理环境
Remove-Item *.exe, *.o

# 2. 重新编译
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
    test_solution.cpp MySolution.cpp -o test_solution.exe

# 3. GLOVE完整测试
$env:OMP_NUM_THREADS=8
.\test_solution.exe ..\data_o\data_o\glove

# 4. 验证指标
# 构建时间 < 2000s
# 召回率@10 ≥ 98%
```

---

## 八、数据集下载链接（参考）

### 官方数据源

如果你的课程没有提供数据，可以尝试以下来源：

1. **GLOVE原始数据**: https://nlp.stanford.edu/projects/glove/
   - 下载 glove.6B.zip（100维词向量）
   - 需要自行转换为本项目格式

2. **SIFT1M数据集**: http://corpus-texmex.irisa.fr/
   - ANN_SIFT1M 数据包
   - 需要转换格式

3. **课程网盘**: 询问助教获取专用下载链接

**注意**: 原始数据集格式与本项目不同，需要编写转换脚本。建议直接从课程平台获取预处理好的数据。

---

## 附录A: 数据格式转换脚本

如果你有二进制格式的SIFT数据，使用以下脚本转换：

```python
# convert_sift_to_txt.py
import struct
import numpy as np

def read_fvecs(filename):
    """读取.fvecs格式文件"""
    with open(filename, 'rb') as f:
        while True:
            # 读取维度
            dim_bytes = f.read(4)
            if not dim_bytes:
                break
            dim = struct.unpack('i', dim_bytes)[0]
            
            # 读取向量
            vec = struct.unpack('f' * dim, f.read(4 * dim))
            yield vec

def convert_fvecs_to_txt(fvecs_file, txt_file):
    """转换.fvecs到.txt"""
    with open(txt_file, 'w') as out:
        for vec in read_fvecs(fvecs_file):
            out.write(' '.join(map(str, vec)) + '\n')
    print(f"✓ Converted: {txt_file}")

# 使用示例
convert_fvecs_to_txt("sift_base.fvecs", "base.txt")
convert_fvecs_to_txt("sift_query.fvecs", "query.txt")
```

---

## 附录B: 快速搭建脚本

将所有步骤合并为一个自动化脚本：

```powershell
# setup_test_env.ps1 - 一键搭建测试环境

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          HNSW测试环境自动搭建工具                           ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# 1. 检查必需文件
Write-Host "[1/4] 检查源文件..." -ForegroundColor Yellow
$required = @("MySolution.h", "MySolution.cpp", "test_solution.cpp")
foreach ($file in $required) {
    if (Test-Path $file) {
        Write-Host "      ✓ $file" -ForegroundColor Green
    } else {
        Write-Host "      ✗ $file 缺失" -ForegroundColor Red
        exit 1
    }
}

# 2. 检查数据集
Write-Host "`n[2/4] 检查数据集..." -ForegroundColor Yellow
if (Test-Path "..\data_o\data_o\glove\base.txt") {
    Write-Host "      ✓ GLOVE数据集存在" -ForegroundColor Green
} else {
    Write-Host "      ⚠ GLOVE数据集不存在" -ForegroundColor Yellow
    Write-Host "        请从课程平台下载并解压到 ..\data_o\ 目录" -ForegroundColor Gray
}

# 3. 编译
Write-Host "`n[3/4] 编译程序..." -ForegroundColor Yellow
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
    test_solution.cpp MySolution.cpp -o test_solution.exe 2>$null

if ($?) {
    Write-Host "      ✓ 编译成功" -ForegroundColor Green
} else {
    Write-Host "      ✗ 编译失败，请检查编译器配置" -ForegroundColor Red
    exit 1
}

# 4. 快速验证
if (Test-Path "..\data_o\data_o\sift_small\base.txt") {
    Write-Host "`n[4/4] 运行快速验证..." -ForegroundColor Yellow
    $env:OMP_NUM_THREADS=8
    .\test_solution.exe ..\data_o\data_o\sift_small 2>&1 | Select-String "Recall"
} else {
    Write-Host "`n[4/4] 跳过验证（无SIFT_SMALL数据集）" -ForegroundColor Yellow
}

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "✓ 环境搭建完成！" -ForegroundColor Green
Write-Host "`n运行完整测试命令:" -ForegroundColor Cyan
Write-Host '  $env:OMP_NUM_THREADS=8' -ForegroundColor White
Write-Host '  .\test_solution.exe ..\data_o\data_o\glove' -ForegroundColor White
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan
```

**使用方法**:
```powershell
.\setup_test_env.ps1
```

---

**恭喜！测试环境搭建完成。** 🎉

现在你可以开始开发和测试HNSW解决方案了。

有任何问题请参考：
- [RECONSTRUCTION_GUIDE.md](RECONSTRUCTION_GUIDE.md) - 重构指南
- [TEST_AND_PACKAGE_GUIDE.md](TEST_AND_PACKAGE_GUIDE.md) - 测试与打包指南
