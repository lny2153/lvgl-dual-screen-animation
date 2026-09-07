@echo off
setlocal
cd /d "%~dp0.."
call C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if errorlevel 1 exit /b 1
"C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B step1/build -G Ninja -DCMAKE_MAKE_PROGRAM=C:/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
"C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build step1/build --target scale_screen_preview --parallel 8
if errorlevel 1 exit /b 1
"C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir step1/build -R screen_preview_pixels --output-on-failure
exit /b %errorlevel%
