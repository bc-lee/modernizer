cmake_minimum_required(VERSION 3.16)

project(modernizer CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_compile_options(-fno-exceptions)
add_compile_options(-fno-rtti)
add_compile_options(-Wall)
add_compile_options(-Wextra)
add_compile_options(-Wno-unused-parameter)
add_compile_options(-Wno-builtin-macro-redefined)
add_compile_options(-Wthread-safety)
add_compile_options(-Werror)
add_definitions(-D__DATE__ -D__TIME__ -D__TIMESTAMP__)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

# third_party/llvm-project
if (NOT (DEFINED LLVM_ENABLE_PROJECTS))
    set(LLVM_ENABLE_PROJECTS "clang")
endif()
set(LLVM_TARGETS_TO_BUILD X86)
set(LLVM_ENABLE_ZLIB OFF)
set(LLVM_INCLUDE_BENCHMARKS OFF)
if (NOT (DEFINED LIBCLANG_BUILD_STATIC))
    set(LIBCLANG_BUILD_STATIC ON)
endif()
set(LLVM_INCLUDE_TESTS OFF)
set(LLVM_INCLUDE_UTILS OFF)
set(CLANG_INCLUDE_TESTS OFF)

# third_party/abseil-cpp
set(ABSL_PROPAGATE_CXX_STD ON)

add_subdirectory(third_party/llvm-project/llvm)
add_subdirectory(third_party/googletest)
add_subdirectory(third_party/abseil-cpp)
add_subdirectory(third_party/re2)

add_subdirectory(modernizer)
add_subdirectory(test)

add_library(libclang_deps INTERFACE)
target_include_directories(libclang_deps INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party/llvm-project/llvm/include
    ${CMAKE_SOURCE_DIR}/third_party/llvm-project/clang/include
    ${CMAKE_BINARY_DIR}/third_party/llvm-project/llvm/include
    ${CMAKE_BINARY_DIR}/third_party/llvm-project/llvm/tools/clang/include
)
target_link_libraries(libclang_deps INTERFACE
    libclang_static clangToolingRefactoring
)

add_library(project_include INTERFACE)
target_include_directories(project_include
    INTERFACE ${CMAKE_SOURCE_DIR}
)
