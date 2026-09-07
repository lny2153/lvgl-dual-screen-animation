#include "GUI_Components.h"
lv_obj_t *gui_group(lv_obj_t *parent,int x,int y,int w,int h) {
 lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
 lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
 lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return o;
}
lv_obj_t *gui_image(lv_obj_t *parent,const lv_image_dsc_t *src,int x,int y) {
 lv_obj_t *o=lv_image_create(parent);lv_image_set_src(o,src);lv_obj_set_pos(o,x,y);
 lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;
}
void gui_visible(lv_obj_t *o,bool v){if(v)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}

