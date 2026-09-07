/* Desktop adapter only: two genuine LVGL displays, one native review window.
 * RGB565 comes from LVGL; Win32 only presents those pixels. No HTML redraw. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "GUI_Screen.h"

typedef struct {
    int w, h, x, y;
    lv_display_t *display;
    uint16_t *draw;
    uint32_t *pixels;
    BITMAPINFO bitmap;
    unsigned flushes;
} panel_t;
static panel_t panels[2];
static gui_screens_t gui;
static HWND window;
static bool calibration;
static int adapter_error;

static uint32_t tick_ms(void) { return (uint32_t)GetTickCount64(); }

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *bytes)
{
    panel_t *p = lv_display_get_user_data(display);
    if(area->x1 < 0 || area->y1 < 0 || area->x2 >= p->w || area->y2 >= p->h) {
        adapter_error = 1;
        lv_display_flush_ready(display);
        return;
    }
    const uint16_t *src = (const uint16_t *)bytes;
    for(int y = area->y1; y <= area->y2; ++y) {
        for(int x = area->x1; x <= area->x2; ++x) {
            uint16_t c = *src++;
            unsigned r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
            p->pixels[y * p->w + x] = (((r << 3) | (r >> 2)) << 16) |
                                      (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
        }
    }
    ++p->flushes;
    lv_display_flush_ready(display);
    if(window) {
        RECT r = {p->x, p->y, p->x + p->w, p->y + p->h};
        InvalidateRect(window, &r, FALSE);
    }
}

static bool create_panel(panel_t *p, int w, int h, int x, int y)
{
    p->w = w; p->h = h; p->x = x; p->y = y;
    p->draw = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)w * 32 * 2);
    p->pixels = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)w * h * 4);
    p->display = lv_display_create(w, h);
    if(!p->draw || !p->pixels || !p->display) return false;
    lv_display_set_user_data(p->display, p);
    lv_display_set_color_format(p->display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(p->display, p->draw, NULL, (uint32_t)(w * 32 * 2), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(p->display, flush);
    p->bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    p->bitmap.bmiHeader.biWidth = w;
    p->bitmap.bmiHeader.biHeight = -h;
    p->bitmap.bmiHeader.biPlanes = 1;
    p->bitmap.bmiHeader.biBitCount = 32;
    p->bitmap.bmiHeader.biCompression = BI_RGB;
    return true;
}

static bool save_bmp(const panel_t *p, const char *path)
{
    FILE *f = NULL;
    if(fopen_s(&f, path, "wb") || !f) return false;
    BITMAPFILEHEADER header = {0};
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    size_t count = (size_t)p->w * p->h;
    header.bfSize = header.bfOffBits + (DWORD)(count * 4);
    bool ok = fwrite(&header, sizeof(header), 1, f) == 1 &&
              fwrite(&p->bitmap.bmiHeader, sizeof(BITMAPINFOHEADER), 1, f) == 1 &&
              fwrite(p->pixels, 4, count, f) == count;
    return fclose(f) == 0 && ok;
}

static bool is_black(const panel_t *p)
{
    for(int i = 0; i < p->w * p->h; ++i) if(p->pixels[i]) return false;
    return true;
}

static bool grid_valid(const panel_t *p)
{
    if(p->pixels[p->w + 1] != 0xFF0000 || p->pixels[2 * p->w - 2] != 0x00FF00 ||
       p->pixels[(p->h - 2) * p->w + 1] != 0x0000FF ||
       p->pixels[(p->h - 1) * p->w - 2] != 0xFFFFFF) return false;
    for(int x = 0; x < p->w; ++x)
        if(p->pixels[x] != 0xFFFFFF || p->pixels[(p->h - 1) * p->w + x] != 0xFFFFFF) return false;
    for(int y = 0; y < p->h; ++y)
        if(p->pixels[y * p->w] != 0xFFFFFF || p->pixels[y * p->w + p->w - 1] != 0xFFFFFF) return false;
    return p->pixels[(p->h / 2) * p->w + p->w / 2] == 0xFFFFFF &&
           p->pixels[10 * p->w + 20] != 0 && p->pixels[10 * p->w + 10] == 0;
}

static int verify(void)
{
    CreateDirectoryW(L"captures", NULL);
    bool ok = true;
    for(int i=0;i<2;i++)lv_refr_now(panels[i].display);
    ok=save_bmp(&panels[0],"captures/left-framework.bmp") && save_bmp(&panels[1],"captures/right-framework.bmp");
    uint32_t stable_right[240*240],stable_left[648*200];
    memcpy(stable_right,panels[1].pixels,sizeof(stable_right));memcpy(stable_left,panels[0].pixels,sizeof(stable_left));
    gui_main_framework(&gui,true);
    for(int i=0;i<2;i++)lv_refr_now(panels[i].display);
    ok=save_bmp(&panels[0],"captures/left-food.bmp") && ok;
    ok=ok && !memcmp(stable_right,panels[1].pixels,sizeof(stable_right));
    for(int y=0;y<200;y++)ok=ok && !memcmp(stable_left+y*648+225,panels[0].pixels+y*648+225,423*4);
    gui_main_framework(&gui,false);
    ok=gui_text_set_values(&gui.components,"12","123","45","678")&&ok;
    lv_refr_now(panels[0].display);save_bmp(&panels[0],"captures/left-dynamic.bmp");
    ok=!gui_text_set_values(&gui.components,"bad","0","0","0")&&ok;
    gui_text_set_values(&gui.components,"0","0","0","0");
    lv_refr_now(panels[0].display);ok=ok && !memcmp(stable_left,panels[0].pixels,sizeof(stable_left));
    gui_visible(gui.components.left,false);gui_visible(gui.components.right,false);
    for(int i = 0; i < 2; ++i) lv_refr_now(panels[i].display);
    ok = ok && is_black(&panels[0]) && is_black(&panels[1]) &&
         save_bmp(&panels[0], "captures/left-blank.bmp") && save_bmp(&panels[1], "captures/right-blank.bmp");
    gui_screen_calibration(gui.left, true);
    gui_screen_calibration(gui.right, true);
    for(int i = 0; i < 2; ++i) lv_refr_now(panels[i].display);
    ok = ok && grid_valid(&panels[0]) && grid_valid(&panels[1]) &&
         save_bmp(&panels[0], "captures/left-grid.bmp") && save_bmp(&panels[1], "captures/right-grid.bmp");
    unsigned right_flushes = panels[1].flushes;
    gui_screen_calibration(gui.left, false);
    for(int i = 0; i < 2; ++i) lv_refr_now(panels[i].display);
    ok = ok && is_black(&panels[0]) && grid_valid(&panels[1]) && right_flushes == panels[1].flushes;
    gui_screens_t rejected = {0};
    ok = ok && !gui_main_create(&rejected, panels[1].display, panels[0].display) &&
         !gui_main_create(&rejected, panels[0].display, panels[0].display) &&
         !gui_main_create(NULL, panels[0].display, panels[1].display) &&
         !gui_main_create(&rejected, NULL, panels[1].display) &&
         !gui_main_create(&rejected, panels[0].display, NULL) &&
         rejected.left == NULL && rejected.right == NULL;
    ok = ok && !adapter_error && panels[0].flushes > 1 && panels[1].flushes > 1;
    FILE *report = NULL;
    if(fopen_s(&report, "captures/validation.txt", "wb") || !report) return 2;
    fprintf(report, "%s\nLVGL 9.5.0 / real RGB565 partial flush / two independent displays\n"
        "Left 648x200; right 240x240; 32-line draw buffers = 56832 bytes\n"
        "Checks: all black pixels; full 1px borders; four corner colors; grid and center;\n"
        "left-only redraw leaves right unchanged; swapped/duplicate/null display rejection; flush bounds.\n"
        "Desktop-only validation. No ESP32 build, SPI transfer, panel initialization or hardware FPS claim.\n",
        ok ? "PASS" : "FAIL");
    fclose(report);
    return ok ? 0 : 1;
}

static void text_at(HDC dc, int x, int y, const wchar_t *text)
{ TextOutW(dc, x, y, text, (int)wcslen(text)); }

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg) {
    case WM_COMMAND:
        if(LOWORD(wp)>=103 && LOWORD(wp)<=107){
            gui_main_framework(&gui,LOWORD(wp)==104);

            if(LOWORD(wp)==106)gui_text_set_values(&gui.components,"12","123","45","678");
            if(LOWORD(wp)==107)gui_text_set_values(&gui.components,"0","0","0","0");
            calibration=false;InvalidateRect(hwnd,NULL,FALSE);return 0;
        }
        if(LOWORD(wp) == 101 || LOWORD(wp) == 102) {

            gui_visible(gui.components.left,false);gui_visible(gui.components.right,false);
            calibration = LOWORD(wp) == 102;
            gui_screen_calibration(gui.left, calibration);
            gui_screen_calibration(gui.right, calibration);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if(wp == 'G' || wp == '1' || wp == '2') {
            bool next = wp == 'G' ? !calibration : wp == '2';
            SendMessageW(hwnd, WM_COMMAND, next ? 102 : 101, 0);
        }
        return 0;
    case WM_TIMER: lv_timer_handler();return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        RECT client; GetClientRect(hwnd, &client);
        HBRUSH bg = CreateSolidBrush(RGB(241, 244, 246));
        FillRect(dc, &client, bg); DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(28, 39, 46));
        HFONT font = CreateFontW(-17, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        HGDIOBJ old = SelectObject(dc, font);
        text_at(dc, 32, 24, L"LVGL · Figma 四组件框架");
        text_at(dc, 32, 105, L"左屏 · 横向 648 × 200 · GC9B72");
        text_at(dc, 720, 105, L"右屏 · 240 × 240 · ST7789V");
        for(int i = 0; i < 2; ++i) {
            panel_t *p = &panels[i];
            StretchDIBits(dc, p->x, p->y, p->w, p->h, 0, 0, p->w, p->h,
                         p->pixels, &p->bitmap, DIB_RGB_COLORS, SRCCOPY);
        }
        text_at(dc, 32, 443, L"此窗口按设备像素 1:1 显示；两屏之间的间距仅用于预览，不代表整机安装间距。");
        SelectObject(dc, old); DeleteObject(font); EndPaint(hwnd, &ps); return 0;
    }
    case WM_DESTROY: KillTimer(hwnd, 1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR args, int show)
{
    (void)previous;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    lv_init(); lv_tick_set_cb(tick_ms);
    if(!create_panel(&panels[0], GUI_LEFT_W, GUI_LEFT_H, 32, 160) ||
       !create_panel(&panels[1], GUI_RIGHT_W, GUI_RIGHT_H, 720, 140) ||
       !gui_main_create(&gui, panels[0].display, panels[1].display)) return 2;
    if(wcscmp(args, L"--verify") == 0) return verify();
    WNDCLASSW cls = {0}; cls.lpfnWndProc = window_proc; cls.hInstance = instance;
    cls.lpszClassName = L"ScaleScreenReview"; cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    if(!RegisterClassW(&cls)) return 3;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT size = {0, 0, 992, 488}; AdjustWindowRectEx(&size, style, FALSE, 0);
    window = CreateWindowW(cls.lpszClassName, L"卡路里秤 · LVGL 四组件框架", style,
        CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left, size.bottom - size.top, NULL, NULL, instance, NULL);
    if(!window) return 3;
    CreateWindowW(L"BUTTON", L"1  纯黑画布", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        32, 58, 140, 32, window, (HMENU)101, instance, NULL);
    CreateWindowW(L"BUTTON", L"2  校准网格", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        184, 58, 140, 32, window, (HMENU)102, instance, NULL);
    const wchar_t *labels[]={L"静态框架",L"品类状态",L"眼睛状态",L"测试数值",L"数值归零"};
    for(int i=0;i<5;i++)CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        338+i*124,58,116,32,window,(HMENU)(INT_PTR)(103+i),instance,NULL);
    ShowWindow(window, show); SetTimer(window, 1, 16, NULL);
    MSG message;
    while(GetMessageW(&message, NULL, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    /* Process exit releases the desktop backing buffers and LVGL objects. */
    return adapter_error ? 1 : 0;
}
