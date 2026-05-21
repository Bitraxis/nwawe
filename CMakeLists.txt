cmake_minimum_required(VERSION 3.21)
project(nwawe LANGUAGES CXX)

option(NWAWE_WITH_GUI "Build the optional GUI frontend" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nwawe_core
    src/runtime.cpp
)

target_include_directories(nwawe_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(nwawe_core PUBLIC cxx_std_20)

add_executable(nwawe
    src/main.cpp
)

target_link_libraries(nwawe PRIVATE nwawe_core)

if (NWAWE_WITH_GUI)
    add_executable(nwawe-gui
        src/gui_raylib.cpp
    )

    find_package(SDL3 QUIET CONFIG)
    if (SDL3_FOUND)
        target_compile_definitions(nwawe-gui PRIVATE NWAWE_USE_SDL3=1)
        target_link_libraries(nwawe-gui PRIVATE nwawe_core SDL3::SDL3)
    else()
        target_link_libraries(nwawe-gui PRIVATE nwawe_core)
    endif()
endif()
