#include "GUI_Screen.h"

bool gui_main_create(gui_screens_t *gui, lv_display_t *left, lv_display_t *right)
{
    /* Validate both panels before touching either one. */
    if(!gui || !left || !right || left == right ||
       lv_display_get_horizontal_resolution(left) != GUI_LEFT_W ||
       lv_display_get_vertical_resolution(left) != GUI_LEFT_H ||
       lv_display_get_horizontal_resolution(right) != GUI_RIGHT_W ||
       lv_display_get_vertical_resolution(right) != GUI_RIGHT_H) return false;
    gui->left = gui_screen_prepare(left);
    gui->right = gui_screen_prepare(right);
    gui_components_t *g=&gui->components;
    g->left=gui_group(gui->left,0,0,648,200);
    g->right=gui_group(gui->right,0,0,240,240);
    gui_text_create(g);gui_eyes_create(g);gui_food_create(g);gui_right_create(g);
    gui_main_framework(gui,false);
    return true;
}

void gui_main_framework(gui_screens_t *gui, bool show_food) {
    gui_components_t *g=&gui->components;
    gui_visible(g->left,true);gui_visible(g->right,true);
    gui_screen_calibration(gui->left,false);gui_screen_calibration(gui->right,false);
    gui_visible(g->eyes,!show_food);
    gui_food_progress(g,show_food?256:0,256,256);
}
