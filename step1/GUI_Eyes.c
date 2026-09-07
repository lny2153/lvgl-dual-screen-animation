#include "GUI_Components.h"
#include "GUI_Assets.h"
void gui_eyes_create(gui_components_t *g){
 g->eyes=gui_group(g->left,0,0,140,200);g->eye_image=gui_image(g->eyes,&gui_eyes_art,0,0);
}
