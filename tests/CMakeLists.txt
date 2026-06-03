cmake_minimum_required(VERSION 3.20)
project(RetroReadTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

add_executable(epub_kernel_test
    epub_kernel_test.cpp
    ../src/epub/EpubKernel.cpp
)

target_include_directories(epub_kernel_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(epub_kernel_test PRIVATE
    TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/hlm"
)

target_link_libraries(epub_kernel_test PRIVATE GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(epub_kernel_test)
