# Minimal CMakeLists for add_subdirectory() consumers.
# If you're integrating as a git submodule, you can ignore this and
# just add `rhrParse/src/rhrParse.cpp` to your own build with
# `rhrParse/include/` on the include path.

cmake_minimum_required(VERSION 3.15)
project(rhrParse CXX)

add_library(rhrParse STATIC src/rhrParse.cpp)
add_library(rhrParse::rhrParse ALIAS rhrParse)

target_include_directories(rhrParse PUBLIC include)
target_compile_features(rhrParse PUBLIC cxx_std_17)
