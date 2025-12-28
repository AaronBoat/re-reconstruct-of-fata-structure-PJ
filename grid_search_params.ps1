# HNSW 参数网格搜索脚本
# 目标：找到使 Recall@10 ≥ 98% 的最优参数组合

param(
    [switch]$Quick = $false  # 快速模式：仅测试关键参数
)

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          HNSW 参数网格搜索 (Grid Search)                    ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# 参数空间定义
if ($Quick) {
    # 快速模式：重点参数
    $M_values = @(30, 40)
    $EF_CONSTRUCTION_values = @(200, 300)
    $GAMMA_values = @(0.5, 1.0)
} else {
    # 完整网格
    $M_values = @(24, 30, 36, 40, 48)
    $EF_CONSTRUCTION_values = @(150, 200, 250, 300, 400)
    $GAMMA_values = @(0.25, 0.5, 0.75, 1.0, 1.5)
}

$EF_SEARCH = 200  # 搜索参数固定

# 结果存储
$results = @()
$best_recall = 0
$best_params = $null

# 计算总测试次数
$total_tests = $M_values.Count * $EF_CONSTRUCTION_values.Count * $GAMMA_values.Count
$current_test = 0

Write-Host "测试配置:" -ForegroundColor Yellow
Write-Host "  M: $($M_values -join ', ')" -ForegroundColor Gray
Write-Host "  EF_CONSTRUCTION: $($EF_CONSTRUCTION_values -join ', ')" -ForegroundColor Gray
Write-Host "  GAMMA: $($GAMMA_values -join ', ')" -ForegroundColor Gray
Write-Host "  总测试数: $total_tests" -ForegroundColor White
Write-Host "  预计耗时: $([math]::Round($total_tests * 8, 1)) 分钟`n" -ForegroundColor Gray

$grid_start_time = Get-Date

# 网格搜索主循环
foreach ($M in $M_values) {
    foreach ($EF_CONST in $EF_CONSTRUCTION_values) {
        foreach ($GAMMA in $GAMMA_values) {
            $current_test++
            
            Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
            Write-Host "测试 [$current_test/$total_tests]" -ForegroundColor Cyan
            Write-Host "  M=$M, EF_CONST=$EF_CONST, GAMMA=$GAMMA" -ForegroundColor White
            
            # 修改源代码参数
            $cpp_content = Get-Content "mysolution.cpp" -Raw
            
            # 使用正则表达式替换参数
            $cpp_content = $cpp_content -replace 'static const int M = \d+;', "static const int M = $M;"
            $cpp_content = $cpp_content -replace 'static const int EF_CONSTRUCTION = \d+;', "static const int EF_CONSTRUCTION = $EF_CONST;"
            $cpp_content = $cpp_content -replace 'static const float GAMMA = [\d.]+f;', "static const float GAMMA = ${GAMMA}f;"
            
            Set-Content "mysolution.cpp" -Value $cpp_content -NoNewline
            
            # 编译
            Write-Host "  编译中..." -ForegroundColor Gray -NoNewline
            $compile_output = g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp test_solution.cpp mysolution.cpp -o test_solution.exe 2>&1
            
            if ($LASTEXITCODE -ne 0) {
                Write-Host " ❌ 编译失败" -ForegroundColor Red
                Write-Host $compile_output
                continue
            }
            Write-Host " ✓" -ForegroundColor Green
            
            # 运行测试
            Write-Host "  测试中..." -ForegroundColor Gray -NoNewline
            $env:OMP_NUM_THREADS = 8
            $test_start = Get-Date
            
            $output = & .\test_solution.exe 2>&1 | Out-String
            
            $test_end = Get-Date
            $test_duration = ($test_end - $test_start).TotalSeconds
            
            # 解析结果
            $build_time = if ($output -match "Build time: (\d+) ms") { [int]$matches[1] / 1000.0 } else { 0 }
            $search_time = if ($output -match "Average search time: ([\d.]+) ms") { [float]$matches[1] } else { 0 }
            $recall_1 = if ($output -match "Recall@1:\s+([\d.]+)") { [float]$matches[1] * 100 } else { 0 }
            $recall_10 = if ($output -match "Recall@10:\s+([\d.]+)") { [float]$matches[1] * 100 } else { 0 }
            
            Write-Host " ✓" -ForegroundColor Green
            
            # 显示结果
            $status_icon = if ($recall_10 -ge 98) { "✅" } elseif ($recall_10 -ge 95) { "⚠️ " } else { "❌" }
            Write-Host "  $status_icon Build: $([math]::Round($build_time, 1))s | Search: ${search_time}ms | R@1: ${recall_1}% | R@10: ${recall_10}%" -ForegroundColor $(if ($recall_10 -ge 98) { "Green" } elseif ($recall_10 -ge 95) { "Yellow" } else { "Red" })
            
            # 记录结果
            $result = [PSCustomObject]@{
                M = $M
                EF_CONSTRUCTION = $EF_CONST
                GAMMA = $GAMMA
                BuildTime = [math]::Round($build_time, 1)
                SearchTime = [math]::Round($search_time, 2)
                Recall1 = [math]::Round($recall_1, 2)
                Recall10 = [math]::Round($recall_10, 2)
                TotalTime = [math]::Round($test_duration, 1)
                Pass = ($recall_10 -ge 98 -and $build_time -le 2000 -and $search_time -le 20)
            }
            $results += $result
            
            # 更新最佳结果
            if ($recall_10 -gt $best_recall) {
                $best_recall = $recall_10
                $best_params = $result
                Write-Host "  🏆 新最佳记录！" -ForegroundColor Magenta
            }
            
            # 如果找到满足条件的参数，可以选择提前退出
            if ($recall_10 -ge 98 -and $build_time -le 2000) {
                Write-Host "`n  ✨ 找到满足要求的参数组合！" -ForegroundColor Green
                # 可选：取消注释以提前退出
                # break
            }
        }
    }
}

$grid_end_time = Get-Date
$total_duration = ($grid_end_time - $grid_start_time).TotalMinutes

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                    网格搜索完成                              ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "总耗时: $([math]::Round($total_duration, 1)) 分钟`n" -ForegroundColor Cyan

# 显示最佳结果
Write-Host "【最佳参数组合】" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "  M = $($best_params.M)" -ForegroundColor White
Write-Host "  EF_CONSTRUCTION = $($best_params.EF_CONSTRUCTION)" -ForegroundColor White
Write-Host "  GAMMA = $($best_params.GAMMA)" -ForegroundColor White
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "  构建时间: $($best_params.BuildTime)s" -ForegroundColor Cyan
Write-Host "  搜索时间: $($best_params.SearchTime)ms" -ForegroundColor Cyan
Write-Host "  Recall@1:  $($best_params.Recall1)%" -ForegroundColor $(if ($best_params.Recall1 -ge 98) { "Green" } else { "Yellow" })
Write-Host "  Recall@10: $($best_params.Recall10)%" -ForegroundColor $(if ($best_params.Recall10 -ge 98) { "Green" } else { "Yellow" })
Write-Host ""

# 显示所有通过测试的参数
$passing_configs = $results | Where-Object { $_.Pass -eq $true } | Sort-Object -Property Recall10 -Descending
if ($passing_configs.Count -gt 0) {
    Write-Host "【通过测试的配置 (Recall@10 ≥ 98%)】" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    $passing_configs | Format-Table -Property M, EF_CONSTRUCTION, GAMMA, BuildTime, SearchTime, Recall1, Recall10 -AutoSize
} else {
    Write-Host "【未找到完全满足要求的配置】" -ForegroundColor Red
    Write-Host "  最佳召回率: $($best_recall)%" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "【Top 5 配置 (按 Recall@10 排序)】" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    $results | Sort-Object -Property Recall10 -Descending | Select-Object -First 5 | Format-Table -Property M, EF_CONSTRUCTION, GAMMA, BuildTime, SearchTime, Recall1, Recall10 -AutoSize
}

# 导出完整结果到CSV
$csv_path = "grid_search_results_$(Get-Date -Format 'yyyyMMdd_HHmmss').csv"
$results | Export-Csv -Path $csv_path -NoTypeInformation -Encoding UTF8
Write-Host "完整结果已保存到: $csv_path" -ForegroundColor Cyan

# 生成参数敏感性分析
Write-Host "`n【参数敏感性分析】" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray

# M 的影响
$m_impact = $results | Group-Object M | ForEach-Object {
    [PSCustomObject]@{
        M = $_.Name
        AvgRecall10 = [math]::Round(($_.Group | Measure-Object -Property Recall10 -Average).Average, 2)
        MaxRecall10 = [math]::Round(($_.Group | Measure-Object -Property Recall10 -Maximum).Maximum, 2)
    }
} | Sort-Object M
Write-Host "M 参数影响:" -ForegroundColor White
$m_impact | Format-Table -AutoSize

# EF_CONSTRUCTION 的影响
$ef_impact = $results | Group-Object EF_CONSTRUCTION | ForEach-Object {
    [PSCustomObject]@{
        EF_CONSTRUCTION = $_.Name
        AvgRecall10 = [math]::Round(($_.Group | Measure-Object -Property Recall10 -Average).Average, 2)
        MaxRecall10 = [math]::Round(($_.Group | Measure-Object -Property Recall10 -Maximum).Maximum, 2)
    }
} | Sort-Object { [int]$_.EF_CONSTRUCTION }
Write-Host "EF_CONSTRUCTION 参数影响:" -ForegroundColor White
$ef_impact | Format-Table -AutoSize

# GAMMA 的影响
$gamma_impact = $results | Group-Object GAMMA | ForEach-Object {
    [PSCustomObject]@{
        GAMMA = $_.Name
        AvgRecall10 = [math]::Round(($_.Group | Measure-Object -Property Recall10 -Average).Average, 2)
        MaxRecall10 = [math]::Round(($_.Group | Measure-Object -Property Recall10 -Maximum).Maximum, 2)
    }
} | Sort-Object { [float]$_.GAMMA }
Write-Host "GAMMA 参数影响:" -ForegroundColor White
$gamma_impact | Format-Table -AutoSize

Write-Host "`n建议:" -ForegroundColor Yellow
if ($best_recall -ge 98) {
    Write-Host "  ✅ 已找到满足要求的参数，建议应用最佳配置" -ForegroundColor Green
    Write-Host "     运行: .\apply_best_params.ps1" -ForegroundColor Cyan
} elseif ($best_recall -ge 97) {
    Write-Host "  ⚠️  最佳召回率接近目标 ($($best_recall)%)" -ForegroundColor Yellow
    Write-Host "     建议: 扩大搜索范围或增加 EF_CONSTRUCTION" -ForegroundColor Cyan
} else {
    Write-Host "  ❌ 召回率仍然较低 ($($best_recall)%)" -ForegroundColor Red
    Write-Host "     建议: 检查算法实现或数据加载逻辑" -ForegroundColor Cyan
}

Write-Host ""
