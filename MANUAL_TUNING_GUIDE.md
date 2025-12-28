# 手动网格搜索 - 逐个测试配置
# 比自动脚本更可控

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          手动参数优化指南                                    ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "当前参数 (基准):" -ForegroundColor Yellow
Write-Host "  M = 30" -ForegroundColor White
Write-Host "  EF_CONSTRUCTION = 200" -ForegroundColor White
Write-Host "  GAMMA = 1.0" -ForegroundColor White
Write-Host "  召回率: 95.9% (需要达到 98%+)" -ForegroundColor $(if (95.9 -ge 98) { "Green" } else { "Yellow" })
Write-Host ""

Write-Host "【建议测试序列】" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host ""

$test_configs = @(
    @{ID=1; M=36; EF=200; G=1.0; Reason="增加M，提高图连通性"},
    @{ID=2; M=40; EF=200; G=1.0; Reason="进一步增加M"},
    @{ID=3; M=30; EF=300; G=1.0; Reason="增加EF_CONSTRUCTION，提高构建质量"},
    @{ID=4; M=30; EF=400; G=1.0; Reason="更高的EF_CONSTRUCTION"},
    @{ID=5; M=36; EF=300; G=1.0; Reason="M和EF都增加"},
    @{ID=6; M=40; EF=300; G=1.0; Reason="更激进的参数组合"},
    @{ID=7; M=30; EF=200; G=0.75; Reason="降低GAMMA，增加多样性"},
    @{ID=8; M=36; EF=250; G=0.75; Reason="综合调整"}
)

foreach ($cfg in $test_configs) {
    Write-Host "[$($cfg.ID)] M=$($cfg.M), EF_CONSTRUCTION=$($cfg.EF), GAMMA=$($cfg.G)" -ForegroundColor Cyan
    Write-Host "    理由: $($cfg.Reason)" -ForegroundColor Gray
}

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host ""

Write-Host "【测试步骤】" -ForegroundColor Yellow
Write-Host "1. 编辑 mysolution.cpp，修改第 11-15 行的参数" -ForegroundColor White
Write-Host "2. 编译: " -NoNewline -ForegroundColor White
Write-Host "g++ -std=c++11 -O3 -mavx2 -mfma -march=native -fopenmp test_solution.cpp mysolution.cpp -o test_solution.exe" -ForegroundColor Cyan
Write-Host "3. 运行: " -NoNewline -ForegroundColor White
Write-Host "`$env:OMP_NUM_THREADS=8; .\test_solution.exe" -ForegroundColor Cyan
Write-Host "4. 记录结果（召回率、构建时间、搜索时间）" -ForegroundColor White
Write-Host "5. 重复测试下一个配置" -ForegroundColor White
Write-Host ""

Write-Host "【记录表格】" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "ID | M  | EF  | GAMMA | Build(s) | Search(ms) | R@1(%)  | R@10(%) | 通过" -ForegroundColor White
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "0  | 30 | 200 | 1.0   | 433.9    | 0.89       | 100.0   | 95.9    | ⚠️" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host ""

Write-Host "【快速修改参数】" -ForegroundColor Yellow
Write-Host "打开 mysolution.cpp，找到第 11-15 行:" -ForegroundColor White
Write-Host ""
Write-Host "  static const int M = 30;              // <- 修改这里" -ForegroundColor Cyan
Write-Host "  static const int EF_CONSTRUCTION = 200; // <- 修改这里" -ForegroundColor Cyan
Write-Host "  static const int EF_SEARCH = 200;" -ForegroundColor Gray
Write-Host "  static const float ML = 1.0f / log(2.0f);" -ForegroundColor Gray
Write-Host "  static const float GAMMA = 1.0f;      // <- 修改这里" -ForegroundColor Cyan
Write-Host ""

Write-Host "【理论分析】" -ForegroundColor Yellow
Write-Host "  • M ↑ = 图更密集 = 召回率 ↑ + 构建时间 ↑" -ForegroundColor White
Write-Host "  • EF_CONSTRUCTION ↑ = 构建质量 ↑ = 召回率 ↑ + 构建时间 ↑" -ForegroundColor White
Write-Host "  • GAMMA ↓ = 多样性 ↑ = 可能提高召回率（不确定）" -ForegroundColor White
Write-Host ""

Write-Host "【预测】" -ForegroundColor Yellow
Write-Host "  最有希望的配置: " -ForegroundColor White
Write-Host "    • [5] M=36, EF_CONSTRUCTION=300 (召回率预计 97-98%)" -ForegroundColor Green
Write-Host "    • [6] M=40, EF_CONSTRUCTION=300 (召回率预计 98-99%)" -ForegroundColor Green
Write-Host ""

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "开始测试吧！每个配置约需 8-12 分钟" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray
