cmake_minimum_required(VERSION 3.10)
set(CMAKE_BUILD_TYPE Release)

project(tas-kid)

add_executable(kidloader src/loader/kidloader.c)
add_library(kiddll SHARED
    src/dll/dllmain.c
    src/dll/hook.c
    src/dll/hook.h
    src/dll/main.c
    src/dll/main.h
    src/dll/offsets.c
    src/dll/offsets.h
    src/dll/tas.c
    src/dll/tas.h
    src/dll/types.h
)

target_link_libraries(kiddll ws2_32)
