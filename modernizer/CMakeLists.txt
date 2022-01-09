add_compile_options(-Wextra)
add_compile_options(-Wno-unused-parameter)
add_compile_options(-Wno-c++20-designator)
add_compile_options(-Wthread-safety)
add_compile_options(-Werror)

add_library(lib_modernizer OBJECT
    diff.cc
    diff.h
    filesystem.cc
    filesystem.h
    modernizer.cc
    modernizer.h
    mutex_lock.h
    path_pattern.cc
    path_pattern.h
)

target_link_libraries(lib_modernizer
    project_include absl::str_format absl::synchronization re2 gtest libclang_deps
)

add_executable(modernizer modernizer_main.cc)

target_link_libraries(modernizer
    project_include lib_modernizer absl::flags absl::flags_parse
)

add_library(test_main OBJECT
    test_main.cc
)

target_link_libraries(test_main
    project_include gtest
)

add_executable(modernizer_test
    diff_unittest.cc
    path_pattern_unittest.cc
)

target_link_libraries(modernizer_test
    project_include lib_modernizer test_main gtest gmock
)
