@echo off
:: 切换命令行编码为 UTF-8，解决中文乱码问题
chcp 65001 >nul

echo ==========================================
echo      IoT 智能监控系统 - 一键启动
echo ==========================================

:: 1. 启动后端 (在一个新窗口中)
:: 这里的 start "Backend" ... 是给新窗口起的名字
:: cmd /k 是让窗口运行完不关闭，方便你看报错
start "Django Backend" cmd /k "call start_backend.bat"

:: 2. 等待 3 秒，让后端先跑起来
timeout /t 3 /nobreak >nul

:: 3. 启动前端 (在另一个新窗口中)
start "React Frontend" cmd /k "call start_frontend.bat"

echo.
echo [成功] 后端和前端已在后台启动！
echo.
echo ------------------------------------------
echo  - 后端窗口：负责数据存储 (请勿关闭)
echo  - 前端窗口：负责网页显示 (请勿关闭)
echo ------------------------------------------
echo.
echo 系统正在运行中... (按任意键退出此引导窗口)
pause >nul