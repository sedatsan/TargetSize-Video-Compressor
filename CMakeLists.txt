# CMakeLists.txt : CMake project for targetsize-video-compressor
cmake_minimum_required (VERSION 3.12)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("targetsize-video-compressor")

# Find dependencies via vcpkg
find_package(FFMPEG REQUIRED)
find_package(imgui CONFIG REQUIRED)

# Fetch Raylib from official GitHub repository to bypass vcpkg busy-wait loop bugs
include(FetchContent)
set(SUPPORT_BUSY_WAIT_LOOP OFF CACHE BOOL "Disable busy wait loop" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable building examples" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        6.0
)
FetchContent_MakeAvailable(raylib)

# Fetch rlImGui
FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(rlimgui)

# Patch rlImGui.h to avoid conflict with Raylib's dynamic linkage macro
set(RLIMGUI_H_PATH "${rlimgui_SOURCE_DIR}/rlImGui.h")
if (EXISTS "${RLIMGUI_H_PATH}")
    file(READ "${RLIMGUI_H_PATH}" RLIMGUI_H_CONTENT)
    string(REPLACE 
        "#elif defined(USE_LIBTYPE_SHARED)"
        "#elif defined(USE_LIBTYPE_SHARED) && !defined(RLIMGUI_STATIC)"
        RLIMGUI_H_CONTENT_FIXED
        "${RLIMGUI_H_CONTENT}"
    )
    file(WRITE "${RLIMGUI_H_PATH}" "${RLIMGUI_H_CONTENT_FIXED}")
endif()


# Add source to this project's executable
add_executable (targetsize-video-compressor 
    "src/main.cpp"
    "src/dialogs.cpp"
    "include/dialogs.hpp"
    "src/TaskQueue.cpp"
    "include/TaskQueue.hpp"
    "include/TaskStatus.hpp"
    "src/ThreadPool.cpp"
    "include/ThreadPool.hpp"
    "src/FFmpegRunner.cpp"
    "include/FFmpegRunner.hpp"
    "${rlimgui_SOURCE_DIR}/rlImGui.cpp"
)

# Set C++23 standard
set_property(TARGET targetsize-video-compressor PROPERTY CXX_STANDARD 23)
set_property(TARGET targetsize-video-compressor PROPERTY CXX_STANDARD_REQUIRED ON)

# Link headers and libraries
target_include_directories(targetsize-video-compressor PRIVATE 
    "include"
    ${FFMPEG_INCLUDE_DIRS}
    ${rlimgui_SOURCE_DIR}
)
target_compile_definitions(targetsize-video-compressor PRIVATE RLIMGUI_STATIC)
target_link_libraries(targetsize-video-compressor PRIVATE 
    ${FFMPEG_LIBRARIES}
    imgui::imgui
    raylib
)

