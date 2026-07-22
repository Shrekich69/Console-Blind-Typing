cmake_minimum_required(VERSION 3.10.0)
project(ConsoleBlindTyping VERSION 1.2.0 LANGUAGES C)

set(CMAKE_BUILD_TYPE Release)

add_executable(cbt src/main.c)
set_target_properties(cbt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ConsBlindTyping/)
