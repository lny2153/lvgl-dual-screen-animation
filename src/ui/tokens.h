#ifndef CALORIE_SCALE_UI_TOKENS_H
#define CALORIE_SCALE_UI_TOKENS_H

#include "lvgl.h"

#define UI_LEFT_W 648
#define UI_LEFT_H 200
#define UI_RIGHT_W 240
#define UI_RIGHT_H 240

#define UI_COLOR_STAGE       lv_color_hex(0x111318)
#define UI_COLOR_PANEL       lv_color_hex(0x1A1D23)
#define UI_COLOR_BLACK       lv_color_hex(0x000000)
#define UI_COLOR_WHITE       lv_color_hex(0xFFFFFF)
#define UI_COLOR_MUTED       lv_color_hex(0x626262)
#define UI_COLOR_LINE        lv_color_hex(0x777777)
#define UI_COLOR_WEIGHT      lv_color_hex(0x85FB49)
#define UI_COLOR_CAL_DARK    lv_color_hex(0xFF9730)
#define UI_COLOR_CAL_LIGHT   lv_color_hex(0xFFCA7F)
#define UI_COLOR_PRO_DARK    lv_color_hex(0x73A8FF)
#define UI_COLOR_PRO_LIGHT   lv_color_hex(0xC6DCFF)
#define UI_COLOR_FAT_DARK    lv_color_hex(0xFFABAB)
#define UI_COLOR_FAT_LIGHT   lv_color_hex(0xFFE6E6)
#define UI_COLOR_CARB_DARK   lv_color_hex(0x73FFFF)
#define UI_COLOR_CARB_LIGHT  lv_color_hex(0xC0FFFC)
#define UI_COLOR_SOD_DARK    lv_color_hex(0xBE8BFF)
#define UI_COLOR_SOD_LIGHT   lv_color_hex(0xDBB7FF)
#define UI_COLOR_SUCCESS     lv_color_hex(0x9DFF9B)
#define UI_COLOR_TARE        lv_color_hex(0xFFB95F)
#define UI_COLOR_RECORD      lv_color_hex(0xB58BFF)
#define UI_COLOR_ALERT       lv_color_hex(0xFF101B)
#define UI_COLOR_LOW         lv_color_hex(0xFFC969)
#define UI_COLOR_CHARGE      lv_color_hex(0x6CFF9B)
#define UI_COLOR_EYE         lv_color_hex(0xA47BFF)

#define UI_MOTION_EASE_X1 512
#define UI_MOTION_EASE_Y1 0
#define UI_MOTION_EASE_X2 0
#define UI_MOTION_EASE_Y2 1024

#define UI_MS_SLOT_OUT       600U
#define UI_MS_SLOT_GAP       140U
#define UI_MS_SLOT_IN        320U
#define UI_MS_NUTRIENT_OUT   400U
#define UI_MS_NUTRIENT_IN    300U
#define UI_MS_SLEEP          800U
#define UI_MS_WAKE           1300U
#define UI_MS_ADD            2730U

LV_FONT_DECLARE(font_ui_18);
LV_FONT_DECLARE(font_ui_24);
LV_FONT_DECLARE(font_units_24);
LV_FONT_DECLARE(font_ui_34);
LV_FONT_DECLARE(font_digits_72);
LV_FONT_DECLARE(font_ui_27);
LV_FONT_DECLARE(font_digits_30);
LV_FONT_DECLARE(font_ui_36);
LV_FONT_DECLARE(font_digits_84);

typedef enum {
    UI_NUTRIENT_CALORIES = 0,
    UI_NUTRIENT_PROTEIN,
    UI_NUTRIENT_FAT,
    UI_NUTRIENT_CARBS,
    UI_NUTRIENT_SODIUM,
    UI_NUTRIENT_COUNT
} ui_nutrient_t;

static inline lv_color_t ui_nutrient_dark(ui_nutrient_t nutrient)
{
    static const uint32_t colors[UI_NUTRIENT_COUNT] = {
        0xFF9730, 0x73A8FF, 0xFFABAB, 0x73FFFF, 0xBE8BFF
    };
    return lv_color_hex(colors[(unsigned)nutrient % UI_NUTRIENT_COUNT]);
}

static inline lv_color_t ui_nutrient_light(ui_nutrient_t nutrient)
{
    static const uint32_t colors[UI_NUTRIENT_COUNT] = {
        0xFFCA7F, 0xC6DCFF, 0xFFE6E6, 0xC0FFFC, 0xDBB7FF
    };
    return lv_color_hex(colors[(unsigned)nutrient % UI_NUTRIENT_COUNT]);
}

#endif
