#pragma once
#include "lvgl.h"
typedef struct {
    lv_obj_t *left, *right, *text, *eyes, *food, *ring, *plus;
    lv_obj_t *values[4], *eye_image, *food_image, *corners, *food_name;
    lv_font_t number_fonts[4], food_font;
} gui_components_t;
lv_obj_t *gui_group(lv_obj_t *parent,int x,int y,int w,int h);
lv_obj_t *gui_image(lv_obj_t *parent,const lv_image_dsc_t *src,int x,int y);
void gui_visible(lv_obj_t *obj,bool visible);
void gui_text_create(gui_components_t *g);
bool gui_text_set_values(gui_components_t *g,const char *total_weight,const char *weight,const char *total_kcal,const char *kcal);
void gui_eyes_create(gui_components_t *g);
void gui_food_create(gui_components_t *g);
void gui_food_progress(gui_components_t *g,int group_scale,int image_scale,int corner_scale);
bool gui_food_set(gui_components_t *g,const lv_image_dsc_t *image,const char *name);
void gui_right_create(gui_components_t *g);
