add_library(calorie_scale_tare_engine STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/tare_engine.c"
)
target_include_directories(calorie_scale_tare_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_compile_features(calorie_scale_tare_engine PUBLIC c_std_11)

add_executable(tare_engine_tests
    tare_engine_tests.c
)
target_link_libraries(tare_engine_tests PRIVATE calorie_scale_tare_engine)
target_compile_features(tare_engine_tests PRIVATE c_std_11)

if(MSVC)
    target_compile_options(calorie_scale_tare_engine PRIVATE /utf-8 /W4 /WX)
    target_compile_options(tare_engine_tests PRIVATE /utf-8 /W4 /WX)
endif()

add_test(NAME tare_engine_tests COMMAND tare_engine_tests)

# Keep the behavior-only state-machine suite in the same CTest entry point.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/transition_rules_tests.c")
    add_executable(transition_rules_tests
        transition_rules_tests.c
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/transition_rules.c"
    )
    target_include_directories(transition_rules_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    )
    target_compile_features(transition_rules_tests PRIVATE c_std_11)
    if(MSVC)
        target_compile_options(transition_rules_tests PRIVATE /utf-8 /W4 /WX)
    endif()
    add_test(NAME transition_rules_tests COMMAND transition_rules_tests)
endif()
