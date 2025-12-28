# 快速参数测试脚本
# 测试单个参数组合，用于快速验证

param(
    [int]$M = 40,
    [int]$EF_CONSTRUCTION = 300,
    [float]$GAMMA = 0.5,
    [switch]$KeepOriginal = $false
)

Write-Host "`n测试参数: M=$M, EF_CONSTRUCTION=$EF_CONSTRUCTION, GAMMA=$GAMMA`n" -ForegroundColor Cyan

# 备份原始文件
if (-not $KeepOriginal) {
    Copy-Item "mysolution.cpp" "mysolution_temp_backup.cpp" -Force
}

# 修改参数
$cpp_content = Get-Content "mysolution.cpp" -Raw
$cpp_content = $cpp_content -replace 'static const int M = \d+;', "static const int M = $M;"
$cpp_content = $cpp_content -replace 'static const int EF_CONSTRUCTION = \d+;', "static const int EF_CONSTRUCTION = $EF_CONSTRUCTION;"
$cpp_content = $cpp_content -replace 'static const float GAMMA = [\d.]+f;', "static const float GAMMA = ${GAMMA}f;"
Set-Content "mysolution.cpp" -Value $cpp_content -NoNewline

# 编译
Write-Host "编译中..." -NoNewline
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp test_solution.cpp mysolution.cpp -o test_solution.exe 2>&1 | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Host " ❌" -ForegroundColor Red
    if (-not $KeepOriginal) {
        Copy-Item "mysolution_temp_backup.cpp" "mysolution.cpp" -Force
        Remove-Item "mysolution_temp_backup.cpp"
    }
    exit 1
}
Write-Host " ✓" -ForegroundColor Green

# 测试
Write-Host "测试中..." -NoNewline
$env:OMP_NUM_THREADS = 8
$output = & .\test_solution.exe 2>&1 | Out-String

# 解析
$build_time = if ($output -match "Build time: (\d+) ms") { [int]$matches[1] / 1000.0 } else { 0 }
$search_time = if ($output -match "Average search time: ([\d.]+) ms") { [float]$matches[1] } else { 0 }
$recall_1 = if ($output -match "Recall@1:\s+([\d.]+)") { [float]$matches[1] * 100 } else { 0 }
$recall_10 = if ($output -match "Recall@10:\s+([\d.]+)") { [float]$matches[1] * 100 } else { 0 }

Write-Host " ✓`n" -ForegroundColor Green

# 显示结果
$status = if ($recall_10 -ge 98) { "✅ 通过" } elseif ($recall_10 -ge 95) { "⚠️  接近" } else { "❌ 失败" }
Write-Host "$status | Build: $([math]::Round($build_time, 1))s | Search: ${search_time}ms | R@1: ${recall_1}% | R@10: ${recall_10}%" -ForegroundColor $(if ($recall_10 -ge 98) { "Green" } elseif ($recall_10 -ge 95) { "Yellow" } else { "Red" })

# 恢复原始文件
if (-not $KeepOriginal) {
    Copy-Item "mysolution_temp_backup.cpp" "mysolution.cpp" -Force
    Remove-Item "mysolution_temp_backup.cpp"
    Write-Host "`n已恢复原始参数" -ForegroundColor Gray
}

Write-Host ""
