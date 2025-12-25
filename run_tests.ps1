# HNSW 测试执行脚本
# 用法: .\run_tests.ps1 [-Quick] [-Full] [-Glove]

param(
    [switch]$Quick,   # 快速测试（SIFT_SMALL，约10秒）
    [switch]$Full,    # 完整测试（GLOVE，约12分钟）
    [switch]$Glove    # 默认GLOVE测试
)

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              HNSW 性能测试工具                              ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# 设置线程数
$env:OMP_NUM_THREADS=8
Write-Host "OpenMP 线程数: 8`n" -ForegroundColor Gray

# 默认执行GLOVE测试
if (-not $Quick -and -not $Full) {
    $Full = $true
}

# 快速测试（SIFT_SMALL）
if ($Quick) {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "快速测试 - SIFT_SMALL (10K向量)" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan
    
    $start = Get-Date
    .\test_solution.exe ..\data_o\data_o\sift_small
    $end = Get-Date
    $duration = ($end - $start).TotalSeconds
    
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "快速测试完成 - 耗时: $([math]::Round($duration, 1))秒" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Cyan
}

# 完整测试（GLOVE）
if ($Full -or $Glove) {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "完整测试 - GLOVE (1.19M × 100维)" -ForegroundColor Yellow
    Write-Host "预计耗时: 约12分钟，请耐心等待..." -ForegroundColor Gray
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
    
    $buildTime = ($testOutput | Select-String "Build time: (\d+) ms").Matches.Groups[1].Value
    $searchTime = ($testOutput | Select-String "Average search time: ([\d.]+) ms").Matches.Groups[1].Value
    $recall1 = ($testOutput | Select-String "Recall@1:\s+([\d.]+)").Matches.Groups[1].Value
    $recall10 = ($testOutput | Select-String "Recall@10:\s+([\d.]+)").Matches.Groups[1].Value
    
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
    }
}

Write-Host "测试脚本执行完毕`n" -ForegroundColor Cyan
