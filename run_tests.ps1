# HNSW GLOVE 数据集测试脚本
# 用法: .\run_tests.ps1

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          HNSW GLOVE 数据集性能测试                          ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# 设置线程数
$env:OMP_NUM_THREADS=8
Write-Host "OpenMP 线程数: 8" -ForegroundColor Gray
Write-Host "数据集: GLOVE (1,192,514 × 100维)" -ForegroundColor Gray
Write-Host "预计耗时: 约12分钟，请耐心等待...`n" -ForegroundColor Yellow

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "开始测试..." -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

$start = Get-Date
$output = .\test_solution.exe ..\data_o\data_o\glove 2>&1 | Tee-Object -Variable testOutput
$end = Get-Date
$duration = ($end - $start).TotalSeconds
    
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "测试完成！" -ForegroundColor Green
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan

# 提取关键指标
Write-Host "【性能指标摘要】" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray

$buildTimeMatch = $testOutput | Select-String "Build time: (\d+) ms"
$buildTime = if ($buildTimeMatch) { $buildTimeMatch.Matches.Groups[1].Value } else { $null }

$searchTimeMatch = $testOutput | Select-String "Average search time: ([\d.]+) ms"
$searchTime = if ($searchTimeMatch) { $searchTimeMatch.Matches.Groups[1].Value } else { $null }

$recall1Match = $testOutput | Select-String "Recall@1:\s+([\d.]+)"
$recall1 = if ($recall1Match) { $recall1Match.Matches.Groups[1].Value } else { $null }

$recall10Match = $testOutput | Select-String "Recall@10:\s+([\d.]+)"
$recall10 = if ($recall10Match) { $recall10Match.Matches.Groups[1].Value } else { $null }

if ($buildTime) {
    $buildTimeSec = [int]$buildTime / 1000
    Write-Host "  构建时间:    ${buildTimeSec}秒 ($buildTime ms)" -ForegroundColor White
    
    if ($buildTimeSec -lt 2000) {
        Write-Host "    状态: ✓ 达标 (< 2000s)" -ForegroundColor Green
    } else {
        Write-Host "    状态: ✗ 超标 (≥ 2000s)" -ForegroundColor Red
    }
}

if ($searchTime) {
    Write-Host "`n  搜索时间:    ${searchTime}ms" -ForegroundColor White
    
    $searchTimeNum = [float]$searchTime
    if ($searchTimeNum -lt 10) {
        Write-Host "    评价: ⭐ 卓越 (< 10ms)" -ForegroundColor Green
    } elseif ($searchTimeNum -lt 20) {
        Write-Host "    评价: ✓ 良好 (< 20ms)" -ForegroundColor Yellow
    } else {
        Write-Host "    评价: ⚠ 可优化 (≥ 20ms)" -ForegroundColor Yellow
    }
}

if ($recall10) {
    $recallPercent = [math]::Round([float]$recall10 * 100, 2)
    Write-Host "`n  召回率@10:   ${recallPercent}%" -ForegroundColor White
    
    if ($recallPercent -ge 98.0) {
        Write-Host "    状态: ✓ 达标 (≥ 98%)" -ForegroundColor Green
    } else {
        Write-Host "    状态: ✗ 未达标 (< 98%)" -ForegroundColor Red
        Write-Host "    差距: $([math]::Round(98.0 - $recallPercent, 2))%" -ForegroundColor Red
    }
}

if ($recall1) {
    $recall1Percent = [math]::Round([float]$recall1 * 100, 2)
    Write-Host "`n  召回率@1:    ${recall1Percent}%" -ForegroundColor White
}

Write-Host "`n  总耗时:      $([math]::Round($duration, 1))秒 ($([math]::Round($duration/60, 2))分钟)" -ForegroundColor Cyan

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

# 综合评定
if ($buildTime -and $recall10) {
    $buildTimeSec = [int]$buildTime / 1000
    $recallPercent = [float]$recall10 * 100
    
    Write-Host "【综合评定】" -ForegroundColor Yellow
    if ($buildTimeSec -lt 2000 -and $recallPercent -ge 98.0) {
        Write-Host "  ✓ 所有硬性指标达标，可以提交！" -ForegroundColor Green
    } else {
        Write-Host "  ✗ 存在指标未达标，需要继续优化" -ForegroundColor Red
    }
    Write-Host ""
} else {
    Write-Host "【警告】" -ForegroundColor Yellow
    if (-not $buildTime) {
        Write-Host "  ⚠ 未能提取构建时间，请检查程序输出" -ForegroundColor Red
    }
    if (-not $recall10) {
        Write-Host "  ⚠ 未能提取召回率，请检查程序输出" -ForegroundColor Red
    }
    Write-Host "`n完整输出请查看上方日志`n" -ForegroundColor Gray
}

Write-Host "测试脚本执行完毕`n" -ForegroundColor Cyan
