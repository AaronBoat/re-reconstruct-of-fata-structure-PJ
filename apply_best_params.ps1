# 应用最佳参数脚本
# 根据网格搜索结果，自动应用最佳参数配置

param(
    [int]$M = 40,
    [int]$EF_CONSTRUCTION = 300,
    [float]$GAMMA = 0.5
)

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              应用最佳参数配置                                ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "目标参数:" -ForegroundColor Yellow
Write-Host "  M = $M" -ForegroundColor White
Write-Host "  EF_CONSTRUCTION = $EF_CONSTRUCTION" -ForegroundColor White
Write-Host "  GAMMA = $GAMMA" -ForegroundColor White
Write-Host ""

# 备份当前文件
$backup_name = "mysolution_backup_$(Get-Date -Format 'yyyyMMdd_HHmmss').cpp"
Copy-Item "mysolution.cpp" $backup_name
Write-Host "✓ 已备份当前文件: $backup_name" -ForegroundColor Green

# 读取并修改参数
$cpp_content = Get-Content "mysolution.cpp" -Raw

$cpp_content = $cpp_content -replace 'static const int M = \d+;', "static const int M = $M;"
$cpp_content = $cpp_content -replace 'static const int EF_CONSTRUCTION = \d+;', "static const int EF_CONSTRUCTION = $EF_CONSTRUCTION;"
$cpp_content = $cpp_content -replace 'static const float GAMMA = [\d.]+f;', "static const float GAMMA = ${GAMMA}f;"

Set-Content "mysolution.cpp" -Value $cpp_content -NoNewline

Write-Host "✓ 参数已更新" -ForegroundColor Green

# 编译
Write-Host "`n编译中..." -ForegroundColor Cyan
$compile_output = g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp test_solution.cpp mysolution.cpp -o test_solution.exe 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ 编译失败！" -ForegroundColor Red
    Write-Host $compile_output
    
    # 恢复备份
    Write-Host "`n恢复备份..." -ForegroundColor Yellow
    Copy-Item $backup_name "mysolution.cpp" -Force
    Write-Host "✓ 已恢复原文件" -ForegroundColor Green
    exit 1
}

Write-Host "✓ 编译成功" -ForegroundColor Green

# 运行验证测试
Write-Host "`n运行验证测试..." -ForegroundColor Cyan
$env:OMP_NUM_THREADS = 8
$output = & .\test_solution.exe 2>&1 | Tee-Object -Variable test_output

# 解析结果
$build_time = if ($test_output -match "Build time: (\d+) ms") { [int]$matches[1] / 1000.0 } else { 0 }
$search_time = if ($test_output -match "Average search time: ([\d.]+) ms") { [float]$matches[1] } else { 0 }
$recall_1 = if ($test_output -match "Recall@1:\s+([\d.]+)") { [float]$matches[1] * 100 } else { 0 }
$recall_10 = if ($test_output -match "Recall@10:\s+([\d.]+)") { [float]$matches[1] * 100 } else { 0 }

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                      验证结果                                ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "性能指标:" -ForegroundColor Yellow
Write-Host "  构建时间: $([math]::Round($build_time, 1))s $(if ($build_time -le 2000) { '✅' } else { '❌ >2000s' })" -ForegroundColor $(if ($build_time -le 2000) { "Cyan" } else { "Red" })
Write-Host "  搜索时间: ${search_time}ms $(if ($search_time -le 20) { '✅' } else { '❌ >20ms' })" -ForegroundColor $(if ($search_time -le 20) { "Cyan" } else { "Red" })
Write-Host "  Recall@1:  $([math]::Round($recall_1, 2))% $(if ($recall_1 -ge 98) { '✅' } else { '⚠️' })" -ForegroundColor $(if ($recall_1 -ge 98) { "Green" } else { "Yellow" })
Write-Host "  Recall@10: $([math]::Round($recall_10, 2))% $(if ($recall_10 -ge 98) { '✅' } else { '❌ <98%' })" -ForegroundColor $(if ($recall_10 -ge 98) { "Green" } else { "Red" })

if ($recall_10 -ge 98 -and $build_time -le 2000 -and $search_time -le 20) {
    Write-Host "`n🎉 所有指标通过！参数应用成功！" -ForegroundColor Green
    Write-Host "   可以删除备份文件: $backup_name" -ForegroundColor Gray
} else {
    Write-Host "`n⚠️  部分指标未达标" -ForegroundColor Yellow
    Write-Host "   备份文件已保留: $backup_name" -ForegroundColor Gray
    Write-Host "   如需恢复: Copy-Item $backup_name mysolution.cpp -Force" -ForegroundColor Cyan
}

Write-Host ""
