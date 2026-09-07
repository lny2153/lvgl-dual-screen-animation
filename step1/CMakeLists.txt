# This directory can also be imported as an ESP-IDF GUI component. The board
# supplies initialized, landscape LVGL displays; no guessed GPIO/init sequence.
set(GUI_SOURCES Main.c GUI_Screen.c GUI_Common.c GUI_Text.c GUI_Eyes.c GUI_Food.c GUI_Right.c GUI_Assets.c)
if(ESP_PLATFORM)
    idf_component_register(SRCS ${GUI_SOURCES} INCLUDE_DIRS "." REQUIRES lvgl)
    return()
endif()
# Same LVGL dependency as the existing project, explicitly no old UI sources.
add_executable(scale_screen_preview WIN32 desktop_win32.c ${GUI_SOURCES})
target_include_directories(scale_screen_preview PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(scale_screen_preview PRIVATE UNICODE _UNICODE)
target_compile_options(scale_screen_preview PRIVATE /utf-8 /W4)
target_link_libraries(scale_screen_preview PRIVATE lvgl user32 gdi32)
set_target_properties(scale_screen_preview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_NAME "双屏基础预览")
add_test(NAME screen_preview_pixels COMMAND scale_screen_preview --verify)
set_tests_properties(screen_preview_pixels PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
