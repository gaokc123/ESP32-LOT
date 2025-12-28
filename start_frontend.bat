@echo off
echo ==========================================
echo        正在启动 React 前端网页...
echo ==========================================

:: 切换到当前脚本所在的目录
cd /d "%~dp0frontend"

:: 方案 A: 尝试使用 Anaconda 环境下的 npm (如果存在)
if exist "E:\Program\Anaconda3\Scripts\npm.cmd" (
    echo [信息] 检测到 Anaconda npm，正在启动...
    call "E:\Program\Anaconda3\Scripts\npm.cmd" run dev
    goto end
)

:: 方案 B: 尝试系统默认 npm
call npm --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [信息] 使用系统默认 npm 启动...
    call npm run dev
    goto end
)

echo [错误] 找不到 npm！
echo 请检查 Node.js 是否安装，或者 E:\Program\Anaconda3\Scripts\npm.cmd 是否存在。

:end
pause