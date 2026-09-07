#include "GUI_Screen.h"

static void rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

lv_obj_t *gui_screen_prepare(lv_display_t *display)
{
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    /* One separate diagnostic overlay. Product components attach to screen,
     * never to this overlay. This also runs on hardware for panel bring-up. */
    lv_obj_t *grid = lv_obj_create(screen);
    lv_obj_remove_style_all(grid);
    int w = lv_display_get_horizontal_resolution(display);
    int h = lv_display_get_vertical_resolution(display);
    lv_obj_set_size(grid, w, h);
    lv_obj_set_pos(grid, 0, 0);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    for(int x = 20; x < w; x += 20) rect(grid, x, 0, 1, h, 0x202830);
    for(int y = 20; y < h; y += 20) rect(grid, 0, y, w, 1, 0x202830);
    rect(grid, 0, 0, w, 1, 0xFFFFFF);
    rect(grid, 0, h - 1, w, 1, 0xFFFFFF);
    rect(grid, 0, 0, 1, h, 0xFFFFFF);
    rect(grid, w - 1, 0, 1, h, 0xFFFFFF);
    rect(grid, w / 2, h / 2 - 8, 1, 17, 0xFFFFFF);
    rect(grid, w / 2 - 8, h / 2, 17, 1, 0xFFFFFF);
    rect(grid, 1, 1, 6, 6, 0xFF0000);           /* top left */
    rect(grid, w - 7, 1, 6, 6, 0x00FF00);       /* top right */
    rect(grid, 1, h - 7, 6, 6, 0x0000FF);       /* bottom left */
    rect(grid, w - 7, h - 7, 6, 6, 0xFFFFFF);   /* bottom right */
    lv_obj_add_flag(grid, LV_OBJ_FLAG_HIDDEN);
    return screen;
}

void gui_screen_calibration(lv_obj_t *screen, bool visible)
{
    /* Created first by gui_screen_prepare; components are added afterward. */
    lv_obj_t *grid = lv_obj_get_child(screen, 0);
    if(visible) {
        lv_obj_remove_flag(grid, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(grid, LV_OBJ_FLAG_HIDDEN);
}
