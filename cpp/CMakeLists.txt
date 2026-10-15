cmake_minimum_required(VERSION 3.20)
project(NativeDipole VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DIPOLE_BUILD_GUI "Build the raylib desktop application" ON)
option(DIPOLE_USE_SYSTEM_RAYLIB "Use an installed raylib 5.5 instead of fetching" OFF)
add_library(dipole_core src/physics.cpp)
target_include_directories(dipole_core PUBLIC include)
if(MSVC)
  target_compile_options(dipole_core PRIVATE /W4)
else()
  target_compile_options(dipole_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
include(CTest)
if(BUILD_TESTING)
  add_executable(dipole_tests tests/core_tests.cpp)
  target_link_libraries(dipole_tests PRIVATE dipole_core)
  add_test(NAME dipole_core_behavior COMMAND dipole_tests)
endif()
if(DIPOLE_BUILD_GUI)
  if(DIPOLE_USE_SYSTEM_RAYLIB)
    find_package(raylib 5.5 EXACT REQUIRED)
  else()
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(USE_AUDIO OFF CACHE BOOL "" FORCE)
    set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
    set(SUPPORT_SCREEN_CAPTURE OFF CACHE BOOL "" FORCE)
    set(SUPPORT_GIF_RECORDING OFF CACHE BOOL "" FORCE)
    set(SUPPORT_CLIPBOARD_IMAGE OFF CACHE BOOL "" FORCE)
    # Prefer a pre-populated local raylib 5.5 tree when one is available, so a
    # bare configure does not have to clone the whole upstream repository. An
    # explicit -DFETCHCONTENT_SOURCE_DIR_RAYLIB=... always wins.
    if(NOT FETCHCONTENT_SOURCE_DIR_RAYLIB
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build-offline/vendor/raylib/CMakeLists.txt")
      set(FETCHCONTENT_SOURCE_DIR_RAYLIB "${CMAKE_CURRENT_SOURCE_DIR}/build-offline/vendor/raylib")
    endif()
    FetchContent_Declare(raylib
      GIT_REPOSITORY https://github.com/raysan5/raylib.git
      GIT_TAG 5.5
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
  endif()
  add_executable(dipole_simulator src/main.cpp src/renderer.cpp src/controls.cpp src/ui_text.cpp)
  target_link_libraries(dipole_simulator PRIVATE dipole_core raylib)
  target_compile_definitions(dipole_simulator PRIVATE NOMINMAX)
  set_target_properties(dipole_simulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
  set(DIPOLE_FONT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/IBMPlexSans-Regular.ttf")
  add_custom_command(TARGET dipole_simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DIPOLE_FONT_FILE}" "$<TARGET_FILE_DIR:dipole_simulator>/IBMPlexSans-Regular.ttf"
    COMMENT "Copying IBM Plex Sans next to dipole_simulator")
endif()
