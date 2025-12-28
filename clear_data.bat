@echo off
:: 切换命令行编码为 UTF-8，解决中文乱码问题
chcp 65001 >nul

echo ==========================================
echo        ⚠️  正在清空数据库所有录音记录...
echo ==========================================

:: 切换到当前脚本所在的目录 (确保路径正确)
cd /d "%~dp0"

:: 方案 A: 尝试使用你电脑上的 Anaconda 绝对路径 (最稳妥)
if exist "E:\Program\Anaconda3\python.exe" (
    echo [信息] 检测到 Anaconda 环境，正在执行清理...
    "E:\Program\Anaconda3\python.exe" clear_data.py
    goto end
)

:: 方案 B: 尝试系统默认 Python
python --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [信息] 使用系统默认 Python 执行清理...
    python clear_data.py
    goto end
)

echo [错误] 找不到 Python！
echo 请检查 E:\Program\Anaconda3\python.exe 是否存在。
pause
exit

:end
echo.
echo ==========================================
echo        ✅ 清理完成！请刷新网页查看。
echo ==========================================
pause