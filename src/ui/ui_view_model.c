#include "ui_view_model.h"

const char *ui_nutrient_name(ui_nutrient_t nutrient)
{
    static const char *names[UI_NUTRIENT_COUNT] = {
        "热量", "蛋白质", "脂肪", "碳水化合物", "钠"
    };
    return names[(unsigned)nutrient % UI_NUTRIENT_COUNT];
}

const char *ui_nutrient_unit(ui_nutrient_t nutrient)
{
    static const char *units[UI_NUTRIENT_COUNT] = {
        "kcal", "g", "g", "g", "mg"
    };
    return units[(unsigned)nutrient % UI_NUTRIENT_COUNT];
}

const char *ui_weight_unit_name(ui_weight_unit_t unit)
{
    static const char *units[] = {"g", "oz", "lb:oz", "ml"};
    return units[(unsigned)unit % 4U];
}
