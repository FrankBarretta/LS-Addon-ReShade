cmake_minimum_required(VERSION 3.15)
project(LS_ReShade)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

# Source files
set(SOURCES
    main.cpp
    logger.cpp
)

# Create the DLL
add_library(LS_ReShade SHARED ${SOURCES})

# Include directories
target_include_directories(LS_ReShade PRIVATE
    ${imgui_SOURCE_DIR}
    ../../LosslessProxy/src
)

# Add ImGui source files
target_sources(LS_ReShade PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

# Link libraries
target_link_libraries(LS_ReShade
    user32
)

# Set output directory
set_target_properties(LS_ReShade PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../addons/LS_ReShade"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../addons/LS_ReShade"
)

# Copy to addons directory after build
add_custom_command(TARGET LS_ReShade POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/../../addons/LS_ReShade"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:LS_ReShade> "${CMAKE_SOURCE_DIR}/../../addons/LS_ReShade/"
    COMMENT "Copying LS_ReShade.dll to addons directory"
)
