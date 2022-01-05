cmake_minimum_required(VERSION 3.16)
project(modernizer CXX)

list(APPEND CMAKE_MODULE_PATH "${LLVM_BUILD_DIR}/share/llvm/cmake")
set(CMAKE_INCLUDE_CURRENT_DIR OFF)

add_compile_options(-fno-exceptions)
add_compile_options(-fno-rtti)
add_compile_options(-Wall)
add_compile_options(-Wno-builtin-macro-redefined)
add_definitions(-D__DATE__ -D__TIME__ -D__TIMESTAMP__)

add_subdirectory(modernizer)

# third_party/abseil-cpp
set(ABSL_PROPAGATE_CXX_STD ON)

add_subdirectory(third_party/googletest)
add_subdirectory(third_party/abseil-cpp)
add_subdirectory(third_party/re2)

add_library(libclang_deps INTERFACE)
target_include_directories(libclang_deps INTERFACE
    ${LLVM_SOURCE_DIR}/include
    ${LLVM_EXTERNAL_CLANG_SOURCE_DIR}/include
    ${LLVM_BINARY_DIR}/include
    ${LLVM_BINARY_DIR}/tools/clang/include
)
target_link_libraries(libclang_deps INTERFACE
    libclang_static clangToolingRefactoring
)

add_library(project_include INTERFACE)
target_include_directories(project_include
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)
