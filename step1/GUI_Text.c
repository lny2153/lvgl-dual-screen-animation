/* Editable LVGL labels. The Figma '0' is calibrated as a MiSans image glyph,
 * NOT a frozen numeric layer. Other digits are generated from the installed MiSans Light.
 * Four phase variants preserve Figma's fractional raster placement for each field. */
#include "GUI_Components.h"
#include "GUI_Assets.h"
#include <string.h>
static const char alphabet[]="0123456789.-";
static const void *bitmap(lv_font_glyph_dsc_t *d,lv_draw_buf_t *b){LV_UNUSED(b);return d->gid.src;}
static bool glyph(const lv_font_t *f,lv_font_glyph_dsc_t *d,uint32_t c,uint32_t next){
 LV_UNUSED(next); const char *p=c<=127?strchr(alphabet,(int)c):NULL;
 if(c>127||c==0||!p)return false;
 const lv_image_dsc_t *const *table=f->dsc;const lv_image_dsc_t *im=table[p-alphabet];
 d->adv_w=im->header.w;d->box_w=im->header.w;d->box_h=im->header.h;
 d->ofs_x=0;d->ofs_y=0;d->format=LV_FONT_GLYPH_FORMAT_IMAGE;d->gid.src=im;return true;
}
void gui_text_create(gui_components_t *g){
 g->text=gui_group(g->left,225,0,423,200);gui_image(g->text,&gui_text_art,0,0);
 const lv_image_dsc_t *const *tables[]={gui_digits_0,gui_digits_1,gui_digits_2,gui_digits_3};
 const int x[]={0,0,241,241},y[]={0,120,0,120},w[]={181,191,176,182},h[]={36,74,36,74};
 for(int i=0;i<4;i++){
  g->number_fonts[i]=(lv_font_t){.get_glyph_dsc=glyph,.get_glyph_bitmap=bitmap,.line_height=h[i],.dsc=tables[i]};
  lv_obj_t *o=g->values[i]=lv_label_create(g->text);lv_obj_remove_style_all(o);
  lv_obj_set_style_text_font(o,&g->number_fonts[i],0);lv_obj_set_style_text_color(o,lv_color_white(),0);
  lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_RIGHT,0);lv_obj_set_pos(o,x[i],y[i]);lv_obj_set_size(o,w[i],h[i]);
  lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text(o,"0");
 }
}
bool gui_text_set_values(gui_components_t *g,const char *a,const char *b,const char *c,const char *d){
 const char *s[]={a,b,c,d};
 /* Reject unsupported characters and overflow as a transaction; never silently clip data. */
 for(int i=0;i<4;i++){
  if(!s[i]||!*s[i])return false;int width=0;
  for(const char *p=s[i];*p;p++){const char *q=strchr(alphabet,*p);if(!q)return false;
   const lv_image_dsc_t *const *t=g->number_fonts[i].dsc;width+=t[q-alphabet]->header.w;}
  if(width>lv_obj_get_width(g->values[i]))return false;
 }
 for(int i=0;i<4;i++)lv_label_set_text(g->values[i],s[i]);return true;
}
