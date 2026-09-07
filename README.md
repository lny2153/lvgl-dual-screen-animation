# 卡路里秤双屏 UI 重建

这是独立的 LVGL Windows 设计验收工程。当前宿主只负责窗口、指针、定时器与 RGB565 渲染环境，不包含任何旧版业务 UI。

## 构建

环境：Windows 10/11、Git，以及安装到 `C:\BuildTools` 的 Visual Studio 2022 C++ Build Tools。首次运行：

```powershell
PowerShell -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
```

随后在项目根目录执行：

```bat
call C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat
C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM=C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release
C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build --parallel
```

产物为 `build\bin\calorie-scale-ui-rebuild.exe`。也可双击工程根目录的 `启动预览.bat`；该脚本会先把工作目录固定到工程根目录，确保 `A:assets/...` 能正确解析。测试接入后可运行：

```bat
C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build --output-on-failure
```

UI 入口契约为：

```c
void ui_app_create(lv_display_t *display);
```

由 `src/ui/ui_app.h` 声明并在 `src/ui/` 中实现。该头文件不存在时，宿主只显示深灰色空舞台，以便单独验证构建链路。宿主窗口为 1120×650；实际 TFT 画布尺寸必须由 UI 层严格保持为左屏 648×200、右屏 240×240。`src/domain/` 与 `src/ui/` 的业务实现不属于宿主层。
