# 快速测试脚本 - 诊断性能问题
# 使用 SIFT_SMALL 快速验证配置

Write-Host "`n==== Quick Diagnostic Test ====" -ForegroundColor Cyan
Write-Host "Dataset: SIFT_SMALL (10K vectors)" -ForegroundColor Yellow
Write-Host "Purpose: Verify build and search performance`n" -ForegroundColor Yellow

$env:OMP_NUM_THREADS = 8

# 编译
Write-Host "[1/3] Compiling..." -ForegroundColor Cyan
g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp `
    test_solution.cpp MySolution.cpp -o test_solution.exe 2>$null

if (-not $?) {
    Write-Host "✗ Compilation failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Compilation OK`n" -ForegroundColor Green

# 运行 SIFT_SMALL
Write-Host "[2/3] Running SIFT_SMALL test..." -ForegroundColor Cyan
$start = Get-Date
.\test_solution.exe ..\data_o\data_o\sift_small 2>&1 | Out-Null
$end = Get-Date
$duration = ($end - $start).TotalSeconds

Write-Host "✓ Test completed in $([math]::Round($duration, 2))s`n" -ForegroundColor Green

# 分析
Write-Host "[3/3] Analysis:" -ForegroundColor Cyan
if ($duration -lt 5) {
    Write-Host "  Status: ✓ FAST - Configuration looks good" -ForegroundColor Green
    Write-Host "  Estimated GLOVE time: ~$([math]::Round($duration * 120, 0))s ($(([math]::Round($duration * 120 / 60, 1)))min)" -ForegroundColor Yellow
} elseif ($duration -lt 10) {
    Write-Host "  Status: ⚠ MODERATE - May be acceptable" -ForegroundColor Yellow
    Write-Host "  Estimated GLOVE time: ~$([math]::Round($duration * 120, 0))s ($(([math]::Round($duration * 120 / 60, 1)))min)" -ForegroundColor Yellow
} else {
    Write-Host "  Status: ✗ SLOW - Configuration needs optimization" -ForegroundColor Red
    Write-Host "  Estimated GLOVE time: >2000s (TIMEOUT RISK)" -ForegroundColor Red
}

Write-Host "`nRecommendations:" -ForegroundColor Cyan
Write-Host "  - Current: M=36, EF_C=250, EF_S=400 (Float)" -ForegroundColor White
Write-Host "  - If slow: Reduce M to 32 or EF_C to 200" -ForegroundColor Yellow
Write-Host "  - If recall low: Increase EF_S to 500" -ForegroundColor Yellow

Write-Host "`n==== Run full test? ====" -ForegroundColor Cyan
Write-Host "Command: .\test_solution.exe ..\data_o\data_o\glove" -ForegroundColor Gray
