cmake_minimum_required(VERSION 2.13)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_library(VisFuzz MODULE
  plugin.cc
)

add_library(VisFuzzDriver STATIC
  driver/afl_driver.cc
  driver/afl-llvm-rt.o.c
)
