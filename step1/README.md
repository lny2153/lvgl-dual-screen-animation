# 四组件静态框架 · LVGL 9.5.0

双击 **双屏基础预览.exe**，在电脑上运行真实 LVGL。左屏 648×200，右屏 240×240。
本轮只验收静态框架：眼睛状态、品类状态、数字更新、屏幕校准；没有动画播放或业务状态机。

## 已验证的像素结果

| 实际 LVGL 输出 | Figma 基准 PNG | 相同 RGB565 格式下差异像素 |
|---|---|---:|
| 眼睛状态左屏 | 229:2，648×200 | **0 / 129600** |
| 加号与显示环右屏 | 229:3，240×240 | **0 / 57600** |
| 品类状态左屏 | 229:2 临时显示隐藏品类 | **0 / 129600** |

结果见 captures/pixel-report.json。图像来自 LVGL 刷新回调，不是网页重画。
比较前将 Figma PNG 转为 RGB565；不能将这个结果解释成 RGB888 原图与 16 位屏幕颜色完全相同。
导出隐藏品类后已恢复 Figma 原有可见性：品类隐藏、眼睛显示。

captures/left-framework.png、right-framework.png、left-food.png 是实际输出。
同目录 *-reference-rgb565.png 为比较基准；*-difference.png 全黑表示零差异。
新数值截图是 left-dynamic.png。更新、拒绝非法输入、归零恢复已验证。
逐像素通过范围仅是上述三张静态图，不扩展到没有参考的新数值组合。

## 四个可复用组件

| 组件 | 文件 | 固定部分 / 更新部分 |
|---|---|---|
| 文字 | GUI_Text.c | 标题、渐变、分隔线与单位为原图；四个数值是独立 LVGL 标签 |
| 眼睛 | GUI_Eyes.c | Figma 渐变、高光原图，保留精确像素位置 |
| 品类 | GUI_Food.c | 四角、食材图、名称为独立对象；本次名称字库只含“苹果” |
| 加号与环 | GUI_Right.c | 两个独立图像对象，同属右屏组件 |

数字使用 MiSans Light 图像字形，由真实 lv_label 排版，支持 0–9、点、负号。
Figma 中的 0 保留各字段的亚像素栅格结果，其余数字由本机 MiSans Light 生成。
不是将数值固化在整张背景中。固定渐变与分数像素位置保存在局部资源里。
所有图片／字形都是编译进 C 的常量，无 PC 文件路径、视频解码器或浏览器依赖。
新增品类时需要扩展名称字库，当前接口不会接受不支持的名称。

## Figma 固定几何

左屏原点为 (0,0)，以下是图层行框坐标；实际着色边界以 PNG 为准。

| 项目 | 位置与尺寸 | 字体 / 样式 |
|---|---|---|
| 双眼整体 | x=3，y=40.160843；133×113.061371 | 原始渐变与高光 |
| 左眼白 | 66.706215×113.061371 | 圆角35.372055 |
| 右眼白 | x=69.455315；66.544685×113.061356 | 圆角35.372051 |
| 虹膜 | 左x=14.023445，右x=79.033791，y=69.839478；47.485775×53.986805 | 径向渐变 |
| 重量标题 | x=225.412842，y=55.706443 | MiSans Demibold 27px，字距1% |
| 热量标题 | x=468.252808，y=55.706443 | MiSans Demibold 27px |
| 大数字 | y=102.027732；重量右边界416.128418，热量右边界647.752304 | MiSans Light 84px |
| 总计数字 | y=-2.999856 | MiSans Light 30px |
| g / kcal | (388.649292,56.637412) / (589.541260,56.449668) | MiSans Normal 27px |
| 食材图标 | (21.693566,21.647302)；89.513611×89.513611 | Figma原图 |
| 食材名称 | (30,151.816586)；71×47 | MiSans Demibold 35.5px，居中 |
| 右屏加号与环整体 | (27,27)；186×186 | 原始加号、灰环与渐变彩环 |

## 固件复用与构建

Main.c 统一创建四组件；GUI_Screen.c 管理两块独立 LVGL 显示器的根节点和校准层。
desktop_win32.c 只用于 PC。CMakeLists.txt 的 ESP-IDF 分支导入相同 GUI C 文件。
后续获准实现动画后，再逐个增加单独 GUI 文件，由 Main 调用。

板端初始化左648×200、右240×240显示器，提供 tick、flush 和任务锁后调用：

```c
static gui_screens_t screens; // 与 GUI 对象保持相同生命周期
if (gui_main_create(&screens, left_display, right_display)) {
    gui_main_framework(&screens, false); // false 眼睛；true 品类
}
// 在 LVGL 线程 / 锁内调用，非法字符或超出宽度返回 false。
gui_text_set_values(&screens.components, "12", "123", "45", "678");
```

已在 Windows 编译运行；尚未 ESP-IDF 编译或实板烧录。
桌面 ../lv_conf.h 的 Windows OS 与16MiB堆不能原样用于 ESP32-S3 N16R8。
公司固件仍需提供引脚、屏幕初始化、旋转偏移与 SPI 配置。
32行RGB565绘制缓冲合计56832字节，不含对象与其他任务；图像字形常量应进入Flash rodata。

构建预览.cmd 使用现有免费 Visual Studio Build Tools 和本地 LVGL 编译并自检。
assets/build_assets.py 使用 Pillow、NumPy、本机 MiSans 生成资源；正常固件构建只需现成 C 文件。
assets/verify_pixels.py 将真实 BMP 转 PNG 并执行逐像素比较。
assets 内 PNG/SVG 是 Figma 原始基准；build 是编译缓存；captures 是验收输出。
build、captures 均不编进固件。验收表.html 保留上一阶段已经审阅的状态记录。

## 给固件团队的包

firmware-package/ 是可直接复制进 ESP-IDF components/scale_gui/ 的最小组件包。
它包含四组件 C 源码、GUI_Assets.c/.h 和组件 CMakeLists.txt，不包含电脑窗口适配器、构建缓存、验收图片或动画。
固件团队只需要把自己的两个 LCD 驱动创建成 lv_display_t，再调用 gui_main_create 与 gui_main_framework。
