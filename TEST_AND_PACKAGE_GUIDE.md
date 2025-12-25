# HNSW项目测试与打包指南

**最后更新**: 2025-12-25  
**适用版本**: 第六批稳定版及后续优化版本

---

## 一、快速测试流程

### 1.1 编译命令

```powershell
# 清理旧文件
Remove-Item -ErrorAction SilentlyContinue test_solution.exe

# 编译（显示警告）
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp -Wall `
    test_solution.cpp MySolution.cpp -o test_solution.exe

# 检查编译结果
if ($?) {
    Write-Host "✓ 编译成功" -ForegroundColor Green
} else {
    Write-Host "✗ 编译失败，请检查错误信息" -ForegroundColor Red
    exit 1
}
```

**关键编译选项说明**:
- `-std=c++11`: C++11标准（必须）
- `-O3`: 最高优化级别（必须）
- `-mavx2 -mfma`: 启用AVX2和FMA指令（性能关键）
- `-march=native`: 针对当前CPU优化（必须）
- `-fopenmp`: 启用OpenMP并行（必须）
- `-Wall`: 显示所有警告（调试用，可选）

---

### 1.2 GLOVE数据集完整测试

```powershell
# 设置并行线程数
$env:OMP_NUM_THREADS=8

# 运行完整测试（约12分钟）
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "开始测试 GLOVE 数据集 (1.19M × 100维)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

$start = Get-Date
.\test_solution.exe ..\data_o\data_o\glove | Tee-Object -Variable output
$end = Get-Date
$duration = ($end - $start).TotalSeconds

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "总耗时: $([math]::Round($duration, 1))秒 ($([math]::Round($duration/60, 2))分钟)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
```

---

### 1.3 快速验证（只看关键指标）

```powershell
# 编译
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
    test_solution.cpp MySolution.cpp -o test_solution.exe 2>$null

if ($?) {
    Write-Host "✓ 编译成功`n" -ForegroundColor Green
    
    # 运行并提取关键指标
    $env:OMP_NUM_THREADS=8
    $start = Get-Date
    $output = .\test_solution.exe ..\data_o\data_o\glove 2>&1
    $end = Get-Date
    
    # 提取性能指标
    $output | Select-String -Pattern "Build time|Average search|Recall@10" | ForEach-Object {
        Write-Host $_ -ForegroundColor Yellow
    }
    
    Write-Host "`n总耗时: $([math]::Round(($end - $start).TotalSeconds/60, 2))分钟" -ForegroundColor Cyan
} else {
    Write-Host "✗ 编译失败" -ForegroundColor Red
}
```

**预期输出示例**:
```
Build time: 400470 ms
Average search time: 17.63 ms
Recall@10: 0.9830
```

---

## 二、性能指标验证

### 2.1 硬性要求检查

```powershell
# 自动验证脚本
function Test-Performance {
    param(
        [string]$OutputFile = "test_result.txt"
    )
    
    # 运行测试并保存输出
    $env:OMP_NUM_THREADS=8
    .\test_solution.exe ..\data_o\data_o\glove > $OutputFile
    
    # 解析结果
    $content = Get-Content $OutputFile
    
    $buildTime = ($content | Select-String "Build time: (\d+) ms").Matches.Groups[1].Value
    $searchTime = ($content | Select-String "Average search time: ([\d.]+) ms").Matches.Groups[1].Value
    $recall10 = ($content | Select-String "Recall@10:\s+([\d.]+)").Matches.Groups[1].Value
    
    # 转换单位
    $buildTimeSec = [int]$buildTime / 1000
    $searchTimeMs = [float]$searchTime
    $recallPercent = [float]$recall10 * 100
    
    # 验证
    Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║              性能指标验证报告                               ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    # 构建时间
    Write-Host "【构建时间】" -ForegroundColor Yellow
    Write-Host "  实际值: $buildTimeSec 秒" -ForegroundColor White
    Write-Host "  要求值: < 2000 秒" -ForegroundColor Gray
    if ($buildTimeSec -lt 2000) {
        Write-Host "  状态: ✓ 通过" -ForegroundColor Green
    } else {
        Write-Host "  状态: ✗ 失败" -ForegroundColor Red
    }
    
    # 召回率
    Write-Host "`n【召回率@10】" -ForegroundColor Yellow
    Write-Host "  实际值: $([math]::Round($recallPercent, 2))%" -ForegroundColor White
    Write-Host "  要求值: ≥ 98.0%" -ForegroundColor Gray
    if ($recallPercent -ge 98.0) {
        Write-Host "  状态: ✓ 通过" -ForegroundColor Green
    } else {
        Write-Host "  状态: ✗ 失败 (差距: $([math]::Round(98.0 - $recallPercent, 2))%)" -ForegroundColor Red
    }
    
    # 搜索时间
    Write-Host "`n【搜索时间】" -ForegroundColor Yellow
    Write-Host "  实际值: $([math]::Round($searchTimeMs, 2)) ms" -ForegroundColor White
    Write-Host "  参考值: < 20 ms (优秀)" -ForegroundColor Gray
    if ($searchTimeMs -lt 10) {
        Write-Host "  状态: ⭐ 卓越" -ForegroundColor Green
    } elseif ($searchTimeMs -lt 20) {
        Write-Host "  状态: ✓ 良好" -ForegroundColor Yellow
    } else {
        Write-Host "  状态: ⚠ 可优化" -ForegroundColor Yellow
    }
    
    # 综合判定
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    if ($buildTimeSec -lt 2000 -and $recallPercent -ge 98.0) {
        Write-Host "【综合评定】✓ 满足所有硬性要求，可提交" -ForegroundColor Green
    } else {
        Write-Host "【综合评定】✗ 未满足要求，需要继续优化" -ForegroundColor Red
    }
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray
}

# 执行验证
Test-Performance
```

---

### 2.2 性能等级评估

| 等级 | 构建时间 | 召回率@10 | 搜索时间 | 综合评价 |
|------|----------|-----------|----------|----------|
| 🏆 **卓越** | <2000s | ≥99.0% | <3ms | 顶级性能 |
| ⭐ **优秀** | <2000s | ≥98.5% | <8ms | 超出预期 |
| ✓ **良好** | <2000s | ≥98.0% | <15ms | 达标有余 |
| ⚠️ **及格** | <2000s | ≥98.0% | <20ms | 刚好达标 |
| ✗ **不及格** | ≥2000s 或 召回率<98.0% | - | 不满足要求 |

**第六批稳定版本**: 400s / 98.3% / 17.63ms → **良好+** (接近优秀)

---

## 三、小规模快速测试

### 3.1 SIFT_SMALL测试（开发阶段）

```powershell
# SIFT_SMALL: 10,000向量 × 128维（约10秒）
Write-Host "快速测试 SIFT_SMALL (10K向量)..." -ForegroundColor Cyan

$env:OMP_NUM_THREADS=8
.\test_solution.exe ..\data_o\data_o\sift_small
```

**用途**: 
- 快速验证代码编译和运行正常
- 检查是否有明显bug或崩溃
- **不能**作为性能评估依据（参数未针对SIFT优化）

---

### 3.2 SIFT完整测试（可选）

```powershell
# SIFT: 1,000,000向量 × 128维（约2-3分钟）
Write-Host "测试 SIFT 数据集 (1M向量)..." -ForegroundColor Cyan

$env:OMP_NUM_THREADS=8
$start = Get-Date
.\test_solution.exe ..\data_o\data_o\sift
$end = Get-Date

Write-Host "`n耗时: $([math]::Round(($end - $start).TotalSeconds, 1))秒" -ForegroundColor Cyan
```

**注意**: SIFT的参数配置与GLOVE不同，性能指标仅供参考

---

## 四、错误诊断与调试

### 4.1 编译错误排查

```powershell
# 详细编译信息（包含警告和错误）
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp -Wall -Wextra `
    test_solution.cpp MySolution.cpp -o test_solution.exe 2>&1 | Tee-Object compile_log.txt

# 查看错误
Get-Content compile_log.txt | Select-String -Pattern "error|warning"
```

**常见编译错误**:

| 错误信息 | 原因 | 解决方法 |
|----------|------|----------|
| `undefined reference to 'omp_*'` | OpenMP未启用 | 添加 `-fopenmp` |
| `unrecognized command line option '-mavx2'` | 编译器不支持AVX2 | 更新g++或移除该选项 |
| `'thread_local' does not name a type` | C++11未启用 | 添加 `-std=c++11` |

---

### 4.2 运行时错误诊断

```powershell
# 捕获崩溃信息
$env:OMP_NUM_THREADS=8
try {
    .\test_solution.exe ..\data_o\data_o\glove 2>&1 | Tee-Object run_log.txt
} catch {
    Write-Host "程序崩溃！错误信息:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Yellow
}

# 检查是否有异常输出
Get-Content run_log.txt | Select-String -Pattern "error|segmentation|abort|terminate"
```

**常见运行时错误**:

| 症状 | 可能原因 | 排查方法 |
|------|----------|----------|
| 立即崩溃 | 数据路径错误 | 检查 `../data_o/data_o/glove` 是否存在 |
| 构建阶段崩溃 | 内存不足或并发bug | 减少线程数: `$env:OMP_NUM_THREADS=4` |
| 搜索阶段崩溃 | 访问越界 | 检查 `final_graph_flat` 索引计算 |
| 结果异常 (recall=0) | 算法逻辑错误 | 对比稳定版本代码 |

---

### 4.3 性能回归检测

```powershell
# 对比脚本
function Compare-Performance {
    param(
        [string]$BaselineFile = "baseline.txt",
        [string]$CurrentFile = "current.txt"
    )
    
    # 提取基准性能
    $baseline = Get-Content $BaselineFile | Select-String "Build time|Average search|Recall@10"
    
    # 运行当前版本
    $env:OMP_NUM_THREADS=8
    .\test_solution.exe ..\data_o\data_o\glove > $CurrentFile
    $current = Get-Content $CurrentFile | Select-String "Build time|Average search|Recall@10"
    
    # 对比显示
    Write-Host "`n【性能对比】" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    Write-Host "基准版本:" -ForegroundColor Yellow
    $baseline | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    Write-Host "`n当前版本:" -ForegroundColor Yellow
    $current | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray
}

# 使用方法
# 1. 首次运行保存基准
.\test_solution.exe ..\data_o\data_o\glove > baseline.txt

# 2. 修改代码后对比
Compare-Performance
```

---

## 五、内存和线程检测

### 5.1 内存泄漏检测（Windows）

```powershell
# 使用Visual Studio内存分析工具
# 或者添加内存监控代码
$before = (Get-Process -Id $PID).WorkingSet64
.\test_solution.exe ..\data_o\data_o\glove > $null
$after = (Get-Process -Id $PID).WorkingSet64

Write-Host "内存使用: $([math]::Round(($after - $before) / 1MB, 2)) MB"
```

---

### 5.2 线程数调优测试

```powershell
# 测试不同线程数的性能
Write-Host "测试不同线程数的性能..." -ForegroundColor Cyan

foreach ($threads in 1, 2, 4, 8) {
    $env:OMP_NUM_THREADS=$threads
    Write-Host "`n线程数: $threads" -ForegroundColor Yellow
    
    $start = Get-Date
    .\test_solution.exe ..\data_o\data_o\glove 2>&1 | Select-String "Build time"
    $end = Get-Date
    
    Write-Host "总耗时: $([math]::Round(($end - $start).TotalSeconds, 1))秒" -ForegroundColor Cyan
}
```

**建议线程数**:
- 物理核心数: 8线程 (推荐)
- 超线程: 16线程 (可能无提升)
- 调试: 1线程 (便于排查问题)

---

## 六、打包提交流程

### 6.1 提交前检查清单

```powershell
function Test-Submission {
    Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║                  提交前检查清单                             ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    $checks = @()
    
    # 1. 检查文件存在
    if (Test-Path "MySolution.cpp") {
        Write-Host "✓ MySolution.cpp 存在" -ForegroundColor Green
        $checks += $true
    } else {
        Write-Host "✗ MySolution.cpp 不存在" -ForegroundColor Red
        $checks += $false
    }
    
    if (Test-Path "MySolution.h") {
        Write-Host "✓ MySolution.h 存在" -ForegroundColor Green
        $checks += $true
    } else {
        Write-Host "✗ MySolution.h 不存在" -ForegroundColor Red
        $checks += $false
    }
    
    # 2. 检查编译
    Write-Host "`n正在编译..." -ForegroundColor Yellow
    g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
        test_solution.cpp MySolution.cpp -o test_solution.exe 2>$null
    
    if ($?) {
        Write-Host "✓ 编译成功" -ForegroundColor Green
        $checks += $true
    } else {
        Write-Host "✗ 编译失败" -ForegroundColor Red
        $checks += $false
    }
    
    # 3. 检查性能
    if ($checks[-1]) {
        Write-Host "`n正在运行性能测试（约12分钟）..." -ForegroundColor Yellow
        $env:OMP_NUM_THREADS=8
        $output = .\test_solution.exe ..\data_o\data_o\glove 2>&1
        
        $buildTime = ($output | Select-String "Build time: (\d+) ms").Matches.Groups[1].Value
        $recall = ($output | Select-String "Recall@10:\s+([\d.]+)").Matches.Groups[1].Value
        
        $buildTimeSec = [int]$buildTime / 1000
        $recallPercent = [float]$recall * 100
        
        if ($buildTimeSec -lt 2000) {
            Write-Host "✓ 构建时间: ${buildTimeSec}s < 2000s" -ForegroundColor Green
            $checks += $true
        } else {
            Write-Host "✗ 构建时间: ${buildTimeSec}s ≥ 2000s" -ForegroundColor Red
            $checks += $false
        }
        
        if ($recallPercent -ge 98.0) {
            Write-Host "✓ 召回率: $([math]::Round($recallPercent, 2))% ≥ 98%" -ForegroundColor Green
            $checks += $true
        } else {
            Write-Host "✗ 召回率: $([math]::Round($recallPercent, 2))% < 98%" -ForegroundColor Red
            $checks += $false
        }
    }
    
    # 4. 综合判定
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    if ($checks -notcontains $false) {
        Write-Host "【检查结果】✓ 所有检查通过，可以打包提交" -ForegroundColor Green
        return $true
    } else {
        Write-Host "【检查结果】✗ 存在问题，请修复后再提交" -ForegroundColor Red
        return $false
    }
}

# 执行检查
$canSubmit = Test-Submission
```

---

### 6.2 打包命令

```powershell
# 清理工作目录
Write-Host "清理临时文件..." -ForegroundColor Yellow
Remove-Item -ErrorAction SilentlyContinue *.exe, *.o, *.obj, *.log, *.txt

# 验证要打包的文件
Write-Host "`n检查打包文件..." -ForegroundColor Yellow
if ((Test-Path "MySolution.cpp") -and (Test-Path "MySolution.h")) {
    Write-Host "✓ 文件完整" -ForegroundColor Green
    
    # 显示文件大小
    $cppSize = (Get-Item "MySolution.cpp").Length
    $hSize = (Get-Item "MySolution.h").Length
    Write-Host "  MySolution.cpp: $([math]::Round($cppSize/1KB, 2)) KB" -ForegroundColor White
    Write-Host "  MySolution.h: $([math]::Round($hSize/1KB, 2)) KB" -ForegroundColor White
} else {
    Write-Host "✗ 文件缺失，无法打包" -ForegroundColor Red
    exit 1
}

# 打包
Write-Host "`n正在打包..." -ForegroundColor Yellow
tar -cf MySolution.tar MySolution.cpp MySolution.h

if ($?) {
    Write-Host "✓ 打包成功: MySolution.tar" -ForegroundColor Green
    
    # 验证打包内容
    Write-Host "`n验证打包内容:" -ForegroundColor Yellow
    tar -tf MySolution.tar | ForEach-Object {
        Write-Host "  $_" -ForegroundColor White
    }
    
    # 显示tar文件大小
    $tarSize = (Get-Item "MySolution.tar").Length
    Write-Host "`n打包文件大小: $([math]::Round($tarSize/1KB, 2)) KB" -ForegroundColor Cyan
} else {
    Write-Host "✗ 打包失败" -ForegroundColor Red
    exit 1
}
```

**预期输出**:
```
✓ 打包成功: MySolution.tar

验证打包内容:
  MySolution.cpp
  MySolution.h

打包文件大小: 42.5 KB
```

---

### 6.3 完整打包脚本（一键操作）

```powershell
# package.ps1 - 一键检查、测试、打包脚本

function Submit-Solution {
    Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║              HNSW解决方案提交准备工具                       ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    # 步骤1: 清理环境
    Write-Host "[1/5] 清理环境..." -ForegroundColor Yellow
    Remove-Item -ErrorAction SilentlyContinue test_solution.exe, *.o, *.log
    Write-Host "      ✓ 完成`n" -ForegroundColor Green
    
    # 步骤2: 编译验证
    Write-Host "[2/5] 编译验证..." -ForegroundColor Yellow
    g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
        test_solution.cpp MySolution.cpp -o test_solution.exe 2>compile.log
    
    if (-not $?) {
        Write-Host "      ✗ 编译失败！" -ForegroundColor Red
        Write-Host "      错误信息:" -ForegroundColor Yellow
        Get-Content compile.log | Select-String "error" | ForEach-Object {
            Write-Host "        $_" -ForegroundColor Red
        }
        return $false
    }
    Write-Host "      ✓ 编译成功`n" -ForegroundColor Green
    
    # 步骤3: 性能测试
    Write-Host "[3/5] 性能测试（约12分钟，请耐心等待）..." -ForegroundColor Yellow
    $env:OMP_NUM_THREADS=8
    $start = Get-Date
    $output = .\test_solution.exe ..\data_o\data_o\glove 2>&1
    $end = Get-Date
    $totalTime = ($end - $start).TotalSeconds
    
    # 提取指标
    $buildTime = ($output | Select-String "Build time: (\d+) ms").Matches.Groups[1].Value
    $searchTime = ($output | Select-String "Average search time: ([\d.]+) ms").Matches.Groups[1].Value
    $recall = ($output | Select-String "Recall@10:\s+([\d.]+)").Matches.Groups[1].Value
    
    $buildTimeSec = [int]$buildTime / 1000
    $searchTimeMs = [float]$searchTime
    $recallPercent = [float]$recall * 100
    
    # 显示结果
    Write-Host "      构建时间: ${buildTimeSec}s" -ForegroundColor White
    Write-Host "      搜索时间: ${searchTimeMs}ms" -ForegroundColor White
    Write-Host "      召回率@10: $([math]::Round($recallPercent, 2))%" -ForegroundColor White
    Write-Host "      总耗时: $([math]::Round($totalTime/60, 2))分钟`n" -ForegroundColor Gray
    
    # 步骤4: 验证要求
    Write-Host "[4/5] 验证性能要求..." -ForegroundColor Yellow
    $passed = $true
    
    if ($buildTimeSec -lt 2000) {
        Write-Host "      ✓ 构建时间达标" -ForegroundColor Green
    } else {
        Write-Host "      ✗ 构建时间超标" -ForegroundColor Red
        $passed = $false
    }
    
    if ($recallPercent -ge 98.0) {
        Write-Host "      ✓ 召回率达标" -ForegroundColor Green
    } else {
        Write-Host "      ✗ 召回率未达标（差距: $([math]::Round(98.0-$recallPercent, 2))%）" -ForegroundColor Red
        $passed = $false
    }
    Write-Host ""
    
    if (-not $passed) {
        Write-Host "性能验证未通过，是否仍要打包？(y/N): " -NoNewline -ForegroundColor Yellow
        $confirm = Read-Host
        if ($confirm -ne "y" -and $confirm -ne "Y") {
            Write-Host "已取消打包" -ForegroundColor Yellow
            return $false
        }
    }
    
    # 步骤5: 打包
    Write-Host "[5/5] 打包文件..." -ForegroundColor Yellow
    Remove-Item -ErrorAction SilentlyContinue MySolution.tar
    tar -cf MySolution.tar MySolution.cpp MySolution.h
    
    if ($?) {
        $tarSize = (Get-Item "MySolution.tar").Length
        Write-Host "      ✓ 打包成功: MySolution.tar ($([math]::Round($tarSize/1KB, 2)) KB)`n" -ForegroundColor Green
        
        # 最终总结
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
        Write-Host "【提交准备完成】" -ForegroundColor Green
        Write-Host "  文件: MySolution.tar" -ForegroundColor White
        Write-Host "  大小: $([math]::Round($tarSize/1KB, 2)) KB" -ForegroundColor White
        Write-Host "  构建时间: ${buildTimeSec}s" -ForegroundColor White
        Write-Host "  召回率: $([math]::Round($recallPercent, 2))%" -ForegroundColor White
        Write-Host "  搜索时间: ${searchTimeMs}ms" -ForegroundColor White
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan
        
        return $true
    } else {
        Write-Host "      ✗ 打包失败`n" -ForegroundColor Red
        return $false
    }
}

# 执行打包流程
Submit-Solution
```

**保存为 `package.ps1`，然后执行**:
```powershell
.\package.ps1
```

---

### 6.4 打包内容验证

```powershell
# 解压验证（可选）
Write-Host "验证tar包内容..." -ForegroundColor Yellow

# 创建临时目录
$tempDir = "temp_verify_$(Get-Random)"
New-Item -ItemType Directory -Path $tempDir | Out-Null

# 解压到临时目录
tar -xf MySolution.tar -C $tempDir

# 检查文件
$files = Get-ChildItem $tempDir
Write-Host "`n解压文件列表:" -ForegroundColor Cyan
$files | ForEach-Object {
    Write-Host "  $($_.Name) - $([math]::Round($_.Length/1KB, 2)) KB" -ForegroundColor White
}

# 检查是否只包含必需文件
$requiredFiles = @("MySolution.cpp", "MySolution.h")
$actualFiles = $files.Name

$onlyRequired = $true
foreach ($file in $actualFiles) {
    if ($file -notin $requiredFiles) {
        Write-Host "`n⚠ 警告: 包含额外文件 $file" -ForegroundColor Yellow
        $onlyRequired = $false
    }
}

foreach ($file in $requiredFiles) {
    if ($file -notin $actualFiles) {
        Write-Host "`n✗ 错误: 缺少必需文件 $file" -ForegroundColor Red
        $onlyRequired = $false
    }
}

if ($onlyRequired -and $actualFiles.Count -eq $requiredFiles.Count) {
    Write-Host "`n✓ 打包内容正确" -ForegroundColor Green
}

# 清理临时目录
Remove-Item -Recurse -Force $tempDir
```

---

## 七、版本备份与管理

### 7.1 创建版本快照

```powershell
# 备份当前版本
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$version = "v1"  # 手动指定版本号

$backupName = "MySolution_${version}_${timestamp}.tar"
tar -cf $backupName MySolution.cpp MySolution.h

Write-Host "✓ 已备份: $backupName" -ForegroundColor Green

# 可选：记录性能指标
$env:OMP_NUM_THREADS=8
$output = .\test_solution.exe ..\data_o\data_o\glove 2>&1
$output | Select-String "Build time|Average search|Recall" > "${version}_performance.txt"

Write-Host "✓ 性能记录: ${version}_performance.txt" -ForegroundColor Green
```

---

### 7.2 版本对比

```powershell
# 对比两个版本的性能
function Compare-Versions {
    param(
        [string]$Version1 = "v1_performance.txt",
        [string]$Version2 = "v2_performance.txt"
    )
    
    Write-Host "`n版本对比:" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    
    Write-Host "`n版本1 ($Version1):" -ForegroundColor Yellow
    Get-Content $Version1 | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    
    Write-Host "`n版本2 ($Version2):" -ForegroundColor Yellow
    Get-Content $Version2 | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray
}
```

---

### 7.3 恢复旧版本

```powershell
# 从备份恢复
function Restore-Version {
    param([string]$BackupFile)
    
    if (-not (Test-Path $BackupFile)) {
        Write-Host "✗ 备份文件不存在: $BackupFile" -ForegroundColor Red
        return
    }
    
    # 备份当前版本
    $currentBackup = "MySolution_before_restore_$(Get-Date -Format 'yyyyMMdd_HHmmss').tar"
    tar -cf $currentBackup MySolution.cpp MySolution.h
    Write-Host "✓ 当前版本已备份: $currentBackup" -ForegroundColor Yellow
    
    # 恢复旧版本
    tar -xf $BackupFile
    Write-Host "✓ 已恢复版本: $BackupFile" -ForegroundColor Green
    
    # 验证编译
    Write-Host "`n验证编译..." -ForegroundColor Yellow
    g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
        test_solution.cpp MySolution.cpp -o test_solution.exe 2>$null
    
    if ($?) {
        Write-Host "✓ 恢复成功，编译通过" -ForegroundColor Green
    } else {
        Write-Host "✗ 恢复失败，编译错误" -ForegroundColor Red
    }
}

# 使用示例
# Restore-Version "MySolution_v6_stable.tar"
```

---

## 八、常用命令速查表

### 快速命令

```powershell
# 1. 快速编译
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp test_solution.cpp MySolution.cpp -o test_solution.exe

# 2. 运行测试
$env:OMP_NUM_THREADS=8; .\test_solution.exe ..\data_o\data_o\glove

# 3. 提取关键指标
.\test_solution.exe ..\data_o\data_o\glove 2>&1 | Select-String "Build time|Average search|Recall@10"

# 4. 打包
tar -cf MySolution.tar MySolution.cpp MySolution.h

# 5. 验证打包
tar -tf MySolution.tar

# 6. 清理
Remove-Item *.exe, *.o, *.log, *.txt
```

---

### 一行命令测试

```powershell
# 编译+测试+提取结果（一行）
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp test_solution.cpp MySolution.cpp -o test_solution.exe 2>$null; if ($?) { $env:OMP_NUM_THREADS=8; .\test_solution.exe ..\data_o\data_o\glove 2>&1 | Select-String "Build time|Average search|Recall" }
```

---

### 批处理脚本保存

将以下内容保存为 `quick_test.ps1`:

```powershell
# quick_test.ps1 - 快速编译测试脚本
param(
    [switch]$Full,  # 完整测试
    [switch]$Fast   # 快速测试（仅编译+关键指标）
)

g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
    test_solution.cpp MySolution.cpp -o test_solution.exe 2>$null

if (-not $?) {
    Write-Host "✗ 编译失败" -ForegroundColor Red
    exit 1
}

Write-Host "✓ 编译成功`n" -ForegroundColor Green

$env:OMP_NUM_THREADS=8

if ($Fast) {
    Write-Host "快速测试模式..." -ForegroundColor Cyan
    .\test_solution.exe ..\data_o\data_o\glove 2>&1 | Select-String "Build time|Average search|Recall@10"
} else {
    Write-Host "完整测试模式..." -ForegroundColor Cyan
    $start = Get-Date
    .\test_solution.exe ..\data_o\data_o\glove
    $end = Get-Date
    Write-Host "`n总耗时: $([math]::Round(($end-$start).TotalSeconds/60, 2))分钟" -ForegroundColor Cyan
}
```

**使用方法**:
```powershell
.\quick_test.ps1 -Fast   # 快速测试
.\quick_test.ps1 -Full   # 完整测试
```

---

## 九、提交检查清单（最终版）

### 打印版检查清单

```
□ 代码文件完整
  □ MySolution.cpp 存在且最新
  □ MySolution.h 存在且最新
  
□ 编译验证
  □ 无编译错误
  □ 无编译警告（或已确认可忽略）
  
□ 性能测试（GLOVE数据集）
  □ 构建时间 < 2000秒: _______秒
  □ 召回率@10 ≥ 98%: _______%
  □ 搜索时间记录: _______ms
  
□ 代码质量
  □ 无明显bug
  □ 关键部分有注释
  □ 无调试输出（或已注释）
  
□ 打包准备
  □ 清理临时文件（*.exe, *.o, *.log）
  □ 打包命令: tar -cf MySolution.tar MySolution.cpp MySolution.h
  □ 验证打包内容: tar -tf MySolution.tar
  
□ 最终确认
  □ MySolution.tar 文件大小合理 (30-60 KB)
  □ 已备份当前版本
  □ 准备提交

签名: __________ 日期: __________
```

---

**祝测试顺利，提交成功！** 🎉
