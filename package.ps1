# 打包脚本 - 创建符合提交要求的 tar 文件
# 格式要求：MySolution.tar 根目录直接包含 MySolution.cpp 和 MySolution.h（不要子文件夹）
# 更新日期：2026-01-02

Write-Host "开始打包 MySolution..." -ForegroundColor Green

# 1. 清理旧文件
if (Test-Path "MySolution.tar") {
    Remove-Item -Force "MySolution.tar"
    Write-Host "已清理旧的 tar 文件" -ForegroundColor Yellow
}

# 2. 验证源文件存在
if (-not (Test-Path "MySolution.cpp")) {
    Write-Host "✗ 错误: MySolution.cpp 不存在" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path "MySolution.h")) {
    Write-Host "✗ 错误: MySolution.h 不存在" -ForegroundColor Red
    exit 1
}

Write-Host "✓ 源文件检查通过" -ForegroundColor Green

# 3. 显示文件信息
$cppSize = (Get-Item "MySolution.cpp").Length
$hSize = (Get-Item "MySolution.h").Length
Write-Host "  MySolution.cpp: $([math]::Round($cppSize/1KB, 2)) KB" -ForegroundColor Cyan
Write-Host "  MySolution.h: $([math]::Round($hSize/1KB, 2)) KB" -ForegroundColor Cyan

# 4. 创建 tar 文件（文件直接放在根目录）
Write-Host "`n正在打包..." -ForegroundColor Yellow
tar -cf MySolution.tar MySolution.cpp MySolution.h

if (Test-Path "MySolution.tar") {
    $tarSize = (Get-Item "MySolution.tar").Length
    Write-Host "✓ 打包成功: MySolution.tar" -ForegroundColor Green
    Write-Host "  文件大小: $([math]::Round($tarSize/1KB, 2)) KB" -ForegroundColor Cyan
    
    # 5. 验证 tar 内容
    Write-Host "`n验证 tar 文件内容:" -ForegroundColor Yellow
    $tarContents = tar -tf MySolution.tar
    $tarContents | ForEach-Object {
        Write-Host "  $_" -ForegroundColor White
    }
    
    # 6. 检查格式是否正确（不应该有子文件夹）
    $hasSubfolder = $tarContents | Where-Object { $_ -like "*/*" -and $_ -notlike "MySolution.*" }
    if ($hasSubfolder) {
        Write-Host "`n⚠ 警告: tar 中包含子文件夹，这可能不符合要求！" -ForegroundColor Red
    } else {
        Write-Host "`n✓ 格式正确：文件直接位于 tar 根目录" -ForegroundColor Green
    }
} else {
    Write-Host "✗ 打包失败" -ForegroundColor Red
    exit 1
}

Write-Host "`n✓ 所有操作完成！可以提交 MySolution.tar" -ForegroundColor Green
