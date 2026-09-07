#include "GUI_Components.h"
#include "GUI_Assets.h"
#include <string.h>
static const void *bitmap(lv_font_glyph_dsc_t *d,lv_draw_buf_t *b){LV_UNUSED(b);return d->gid.src;}
static bool glyph(const lv_font_t *f,lv_font_glyph_dsc_t *d,uint32_t c,uint32_t next){
 LV_UNUSED(f);LV_UNUSED(next);
 const lv_image_dsc_t *im=c==0x82f9?&gui_food_char_33529:c==0x679c?&gui_food_char_26524:NULL;
 if(!im)return false;d->adv_w=im->header.w;d->box_w=im->header.w;d->box_h=im->header.h;
 d->ofs_x=0;d->ofs_y=0;d->format=LV_FONT_GLYPH_FORMAT_IMAGE;d->gid.src=im;return true;
}
void gui_food_create(gui_components_t *g){
 g->food=gui_group(g->left,0,0,140,200);
 g->corners=gui_image(g->food,&gui_corners_art,0,0);
 g->food_image=gui_image(g->food,&gui_food_art,0,0);
 lv_image_set_pivot(g->corners,67,66);lv_image_set_pivot(g->food_image,67,66);
 g->food_font=(lv_font_t){.get_glyph_dsc=glyph,.get_glyph_bitmap=bitmap,.line_height=47};
 g->food_name=lv_label_create(g->food);lv_obj_remove_style_all(g->food_name);
 lv_obj_set_style_text_font(g->food_name,&g->food_font,0);
 lv_obj_set_style_text_color(g->food_name,lv_color_white(),0);
 lv_obj_set_style_text_align(g->food_name,LV_TEXT_ALIGN_CENTER,0);
 lv_obj_set_pos(g->food_name,30,152);lv_obj_set_size(g->food_name,71,47);lv_label_set_text(g->food_name,"苹果");
 lv_obj_set_style_transform_pivot_x(g->food,67,0);lv_obj_set_style_transform_pivot_y(g->food,100,0);
 gui_visible(g->food,false);
}
void gui_food_progress(gui_components_t *g,int a,int b,int c){
 gui_visible(g->food,a>0);lv_obj_set_style_transform_scale(g->food,a>0?a:1,0);
 lv_image_set_scale(g->food_image,b>0?b:1);lv_image_set_scale(g->corners,c>0?c:1);
 gui_visible(g->food_image,b>0);gui_visible(g->corners,c>0);
}
bool gui_food_set(gui_components_t *g,const lv_image_dsc_t *im,const char *name){
 /* This first approved font subset contains only 苹果. Expand the subset when adding food names. */
 if(!im||!name||strcmp(name,"苹果"))return false;
 lv_image_set_src(g->food_image,im);lv_label_set_text(g->food_name,name);return true;
}
