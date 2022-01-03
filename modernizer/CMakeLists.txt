add_library(lib_modernizer OBJECT
    diff.cc
    diff.h
    filesystem.cc
    filesystem.h
    mutex_lock.h
    modernizer.cc
    modernizer.h
    refactoring.cc
    refactoring.h
)

target_compile_options(lib_modernizer PRIVATE
    -Wthread-safety
)

target_link_libraries(lib_modernizer
    project_include absl::cleanup absl::str_format absl::synchronization re2 gtest libclang_deps
)

add_executable(modernizer modernizer_main.cc)

target_link_libraries(modernizer
    project_include lib_modernizer absl::flags absl::flags_parse
)

add_executable(modernizer_test
    modernizer_unittest.cc
)

target_compile_definitions(modernizer_test
    PRIVATE -DTESTDATA_ROOT="${CMAKE_SOURCE_DIR}/test/data"
)

target_link_libraries(modernizer_test
    project_include lib_modernizer test_main gtest gmock
)
