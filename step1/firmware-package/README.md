# scale_gui · ESP-IDF 组件包

把本目录复制到固件工程的 components/scale_gui/。不要复制 desktop_win32.c，它只属于电脑预览。

这个组件只负责 LVGL 对象和资源，不负责屏幕驱动。固件必须先完成：

1. 初始化 LVGL；
2. 分别创建并配置左屏 648×200、右屏 240×240 的 lv_display_t；
3. 设置 RGB565 色彩格式、flush 回调、tick 和绘制缓冲；
4. 确认旋转、RGB/BGR、SPI 字节序、窗口偏移与背光；
5. 在 LVGL 线程或任务锁内调用 GUI。

最小调用：

    #include "GUI_Screen.h"
    static gui_screens_t screens;
    bool gui_ready = gui_main_create(&screens, left_display, right_display);
    if(gui_ready) {
        gui_main_framework(&screens, false); /* false=眼睛，true=品类 */
        gui_text_set_values(&screens.components, "0", "0", "0", "0");
    }

gui_main_create 会检查两个显示器的分辨率、空指针和重复显示器；检查失败时不会创建半套对象。
数字更新支持 0–9、.、-；非法字符或超过字段宽度返回 false。

## 包内文件

Main.c：统一创建两屏和四个组件。
GUI_Screen.c/.h：屏幕根节点、黑底和校准层。
GUI_Components.h：四组件公共数据结构和接口。
GUI_Common.c：可复用 LVGL 容器、图像、显隐辅助函数。
GUI_Text.c：固定文字背景与可更新数字。
GUI_Eyes.c：Figma 眼睛状态。
GUI_Food.c：Figma 品类状态。
GUI_Right.c：右屏加号和显示环。
GUI_Assets.c/.h：编译进固件的 RGB565 图像和字形常量。
CMakeLists.txt：ESP-IDF 组件声明。

GUI_Assets.c 的常量会进入固件只读区，约 1.6 MB；请在分区表中为应用和只读资源保留足够空间。它不包含动画帧。

## 与电脑预览的关系

电脑预览使用同一组 Main.c、GUI_Screen.c 和组件 C 文件，只额外编译 desktop_win32.c 作为显示适配器。
电脑通过 RGB565 刷新回调显示；TFT 通过你的 SPI/LCD flush 回调显示。因此组件代码可以复用，显示驱动不能直接从电脑包搬过去。

## 实屏首次验收顺序

先只显示黑底和校准层，确认四边、中心、四角颜色及屏幕方向；再显示眼睛；再显示品类；最后测试数字。
如果整张图发生固定平移，优先检查 LCD set_window 的起点和旋转；如果颜色互换，检查 RGB/BGR 与字节序；如果只有边缘颜色不同，检查面板色彩格式和 SPI 发送顺序。

本包是静态框架，未包含 M004 或其他动画，也没有虚构任何 GPIO 和面板初始化参数。
