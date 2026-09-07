#include <Windows.h>

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include "lvgl.h"

#if defined(CALORIE_SCALE_HAS_UI_ENTRYPOINT)
#include "ui/ui_app.h"
#endif

enum {
    HOST_WINDOW_WIDTH = 1120,
    HOST_WINDOW_HEIGHT = 650,
    HOST_MAX_DIRECTORY_LENGTH = 32768,
    HOST_PARENT_SEARCH_DEPTH = 5
};

static bool directory_contains_assets(const wchar_t *directory)
{
    wchar_t assets_path[HOST_MAX_DIRECTORY_LENGTH];
    int written = swprintf_s(assets_path, HOST_MAX_DIRECTORY_LENGTH, L"%ls\\assets", directory);
    if(written <= 0) return false;

    DWORD attributes = GetFileAttributesW(assets_path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
}

static void select_project_working_directory(void)
{
    wchar_t directory[HOST_MAX_DIRECTORY_LENGTH];
    DWORD length = GetModuleFileNameW(NULL, directory, HOST_MAX_DIRECTORY_LENGTH);
    if(length == 0U || length >= HOST_MAX_DIRECTORY_LENGTH) return;

    wchar_t *slash = wcsrchr(directory, L'\\');
    if(!slash) return;
    *slash = L'\0';

    for(int depth = 0; depth < HOST_PARENT_SEARCH_DEPTH; ++depth) {
        if(directory_contains_assets(directory)) {
            SetCurrentDirectoryW(directory);
            return;
        }

        slash = wcsrchr(directory, L'\\');
        if(!slash) return;
        *slash = L'\0';
    }
}

static void prepare_host_stage(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x17191D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE previous_instance,
    _In_ LPWSTR command_line,
    _In_ int show_command)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    select_project_working_directory();
    lv_init();

    const int32_t zoom_percent = 100;
    const bool allow_dpi_override = false;
    const bool simulator_mode = true;
    lv_display_t *display = lv_windows_create_display(
        L"卡路里秤 · 双屏界面重建",
        HOST_WINDOW_WIDTH,
        HOST_WINDOW_HEIGHT,
        zoom_percent,
        allow_dpi_override,
        simulator_mode);
    if(!display) return -1;

    lv_indev_t *pointer = lv_windows_acquire_pointer_indev(display);
    if(!pointer) return -2;
    lv_indev_set_display(pointer, display);

    HWND window_handle = lv_windows_get_display_window_handle(display);
    if(!window_handle) return -3;

    lv_display_set_default(display);
    prepare_host_stage();

#if defined(CALORIE_SCALE_HAS_UI_ENTRYPOINT)
    ui_app_create(display);
#endif

    while(IsWindow(window_handle)) {
        uint32_t wait_ms = lv_timer_handler();
        if(wait_ms == LV_NO_TIMER_READY) wait_ms = LV_DEF_REFR_PERIOD;
        if(wait_ms > 20U) wait_ms = 20U;
        lv_delay_ms(wait_ms);
    }

    return 0;
}
