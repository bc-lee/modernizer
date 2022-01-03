add_library(test_main OBJECT
    test_main.cc
)

target_link_libraries(test_main
    project_include gtest libclang_deps
)
