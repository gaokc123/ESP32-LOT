@echo off
echo ==========================================
echo        正在启动 Django 后端服务器...
echo ==========================================

:: 切换到当前脚本所在的目录
cd /d "%~dp0"

:: 方案 A: 尝试使用你电脑上的 Anaconda 绝对路径 (最稳妥)
if exist "E:\Program\Anaconda3\python.exe" (
    echo [信息] 检测到 Anaconda 环境，正在启动...
    "E:\Program\Anaconda3\python.exe" manage.py runserver 0.0.0.0:8000
    goto end
)

:: 方案 B: 如果上面没找到，尝试系统默认 Python
python --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [信息] 使用系统默认 Python 启动...
    python manage.py runserver 0.0.0.0:8000
    goto end
)

echo [错误] 找不到 Python！
echo 请检查 E:\Program\Anaconda3\python.exe 是否存在。

:end
:: 如果服务器意外关闭，暂停显示错误信息
pause