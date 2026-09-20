@echo off
rem 灵王工具箱 Windows 一键构建：编译 + windeployqt + 安装包
rem 前提：已安装 CMake、Ninja、Qt 6.8 (win64_mingw) 及配套 MinGW、Inno Setup 6
setlocal

set QT_DIR=C:\Qt\6.8.2\mingw_64
set MINGW_DIR=C:\Qt\Tools\mingw1310_64
set PATH=%MINGW_DIR%\bin;%QT_DIR%\bin;%PATH%

cd /d "%~dp0.."

echo [1/4] CMake 配置...
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 || goto :err

echo [2/4] 编译...
cmake --build build || goto :err

echo [3/4] 部署运行时...
if exist dist rmdir /s /q dist
mkdir dist
copy build\lingwtools.exe dist\ >nul
windeployqt --release --no-translations --compiler-runtime dist\lingwtools.exe || goto :err

echo [4/4] 生成安装包...
where ISCC >nul 2>nul
if %errorlevel%==0 (
    ISCC scripts\installer.iss || goto :err
) else (
    "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" scripts\installer.iss || goto :err
)

echo.
echo 完成！产物：
echo   dist\lingwtools.exe               （便携版）
echo   lingwtools-setup-1.0.2.exe        （安装向导）
exit /b 0

:err
echo 构建失败，请检查上方错误信息。
exit /b 1
