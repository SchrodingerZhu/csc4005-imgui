cmake_minimum_required(VERSION 3.19)
project(csc4005_imgui)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)


include_directories(include imgui imgui/backends ${SDL2_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS} Vulkan::Headers)
file(GLOB IMGUI_SRC
        imgui/*.cpp
        imgui/backends/imgui_impl_sdl.cpp
        imgui/backends/imgui_impl_vulkan.cpp
        imgui/misc/freetype/imgui_freetype.cpp
        imgui/misc/fonts/binary_to_compressed_c.cpp
        imgui/misc/cpp/imgui_stdlib.cpp
        )
add_library(imgui STATIC ${IMGUI_SRC})
file(GLOB CSC4005_PROJECT_SRC src/*.cpp src/*.c)
add_executable(csc4005_imgui ${CSC4005_PROJECT_SRC})
target_link_libraries(imgui PRIVATE Freetype::Freetype SDL2::SDL2 Vulkan::Vulkan)
target_link_libraries(csc4005_imgui imgui)
target_compile_options(csc4005_imgui PRIVATE -Werror -Wall -Wextra -Wpedantic)