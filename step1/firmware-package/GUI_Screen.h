#ifndef SCALE_GUI_SCREEN_H
#define SCALE_GUI_SCREEN_H
#include "lvgl.h"
#include <stdbool.h>
#include "GUI_Components.h"

/* Logical installation orientation, confirmed by the designer. */
enum { GUI_LEFT_W = 648, GUI_LEFT_H = 200, GUI_RIGHT_W = 240, GUI_RIGHT_H = 240 };
typedef struct { lv_obj_t *left; lv_obj_t *right; gui_components_t components; } gui_screens_t;
void gui_main_framework(gui_screens_t *gui, bool show_food);

/* Board/desktop owns display drivers, buffers, tick and LVGL initialization.
 * Call these on the LVGL thread (or under the board's LVGL lock).
 * Main creates once; future GUI_Mxxx.c animation modules are called from Main.c. */
bool gui_main_create(gui_screens_t *gui, lv_display_t *left, lv_display_t *right);
lv_obj_t *gui_screen_prepare(lv_display_t *display);
void gui_screen_calibration(lv_obj_t *screen, bool visible);
#endif
