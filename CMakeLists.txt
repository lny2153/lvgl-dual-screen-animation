cmake_minimum_required(VERSION 3.20)

project(calorie_scale_dual_screen_ui_rebuild VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "The design-review host currently targets the LVGL Windows driver.")
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/lvgl")
if(NOT EXISTS "${LVGL_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "LVGL v9.5.0 is missing. Run scripts/bootstrap.ps1 first.")
endif()

set(LV_BUILD_CONF_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "LVGL configuration directory" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

add_subdirectory(external/lvgl)

target_compile_definitions(lvgl PUBLIC
    LV_CONF_INCLUDE_SIMPLE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
)
target_include_directories(lvgl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Product code is discovered without encoding old screen composition in the
# build system. src/main.c is the only host-owned source file.
file(GLOB_RECURSE CALORIE_SCALE_IMPLEMENTATION_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
list(REMOVE_ITEM CALORIE_SCALE_IMPLEMENTATION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
)

add_executable(calorie_scale_ui_rebuild WIN32
    src/main.c
    ${CALORIE_SCALE_IMPLEMENTATION_SOURCES}
)
target_include_directories(calorie_scale_ui_rebuild PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(calorie_scale_ui_rebuild PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    UNICODE
    _UNICODE
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/ui_app.h")
    target_compile_definitions(calorie_scale_ui_rebuild PRIVATE
        CALORIE_SCALE_HAS_UI_ENTRYPOINT=1
    )
endif()

if(MSVC)
    target_compile_options(calorie_scale_ui_rebuild PRIVATE /utf-8 /W4)
endif()

target_link_libraries(calorie_scale_ui_rebuild PRIVATE lvgl)

set_target_properties(calorie_scale_ui_rebuild PROPERTIES
    OUTPUT_NAME "calorie-scale-ui-rebuild"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

enable_testing()
add_subdirectory(step1)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
