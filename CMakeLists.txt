cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME LCD)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# --- LVGL Configuration ---
# 1. Include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/LVGL
    ${CMAKE_SOURCE_DIR}/LVGL/src
    ${CMAKE_SOURCE_DIR}/LVGL/porting
    ${CMAKE_SOURCE_DIR}/LVGL/src/extra
)

# 2. Source files
file(GLOB_RECURSE LVGL_SOURCES
    "${CMAKE_SOURCE_DIR}/LVGL/src/*.c"
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${LVGL_SOURCES}
    ${CMAKE_SOURCE_DIR}/LVGL/porting/lv_port_disp.c
    ${CMAKE_SOURCE_DIR}/LVGL/porting/lv_port_indev.c
    ${CMAKE_SOURCE_DIR}/LVGL/porting/lv_port_fs.c
)

# --- Hardware Configuration ---
# 1. Include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Handware
    ${CMAKE_SOURCE_DIR}/LCD
    ${CMAKE_SOURCE_DIR}/Core/Inc
)

# 2. Source files
file(GLOB HARDWARE_SOURCES
    "${CMAKE_SOURCE_DIR}/Handware/*.c"
    "${CMAKE_SOURCE_DIR}/LCD/*.c"
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${HARDWARE_SOURCES}
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)


# 缂栬瘧瀹屾垚鍚庯紝鎶婄敓鎴愮殑 elf 鏂囦欢寮哄埗澶嶅埗鍒颁竴涓笉浼氳 CLion 灞忚斀鐨?bin 鏂囦欢澶?
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_SOURCE_DIR}/bin/${PROJECT_NAME}.elf
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Auto-syncing ELF to local Windows and printing sizes..."
)
