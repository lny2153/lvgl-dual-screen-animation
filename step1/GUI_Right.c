#include "GUI_Components.h"
#include "GUI_Assets.h"
/* One component, two independently addressable children. M004 never mutates either. */
void gui_right_create(gui_components_t *g){
 g->ring=gui_image(g->right,&gui_ring_art,0,0);
 g->plus=gui_image(g->right,&gui_plus_art,76,77);
}

