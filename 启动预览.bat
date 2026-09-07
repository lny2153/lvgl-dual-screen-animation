@echo off
setlocal
chcp 65001 >nul
pushd "%~dp0"

if not exist "build\bin\calorie-scale-ui-rebuild.exe" (
  echo [ERROR] Preview executable not found.
  echo Build the project using the README instructions first.
  popd
  exit /b 1
)

start "" ".\build\bin\calorie-scale-ui-rebuild.exe"
popd
exit /b 0
