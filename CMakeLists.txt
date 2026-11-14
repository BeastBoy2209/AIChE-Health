cmake_minimum_required(VERSION 3.16)
project(AIChEHealth CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  add_compile_options(/utf-8 /W3 /permissive- /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ---- sqlite (vendored amalgamation)
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
if(NOT MSVC)
  target_link_libraries(sqlite3 PUBLIC dl pthread)
endif()

# ---- core library (storage, scoring, advisor, REST server)
add_library(aiche_core STATIC core/catalog.cpp core/engine.cpp core/server.cpp)
target_include_directories(aiche_core PUBLIC core third_party)
target_link_libraries(aiche_core PUBLIC sqlite3)
if(WIN32)
  target_link_libraries(aiche_core PUBLIC ws2_32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(aiche_core PUBLIC Threads::Threads)
endif()

add_executable(aiche-core core/main.cpp)
target_link_libraries(aiche-core PRIVATE aiche_core)

# ---- native UI (Dear ImGui + GLFW + OpenGL)
option(ASTRO_BUILD_UI "Build the native UI" ON)
if(ASTRO_BUILD_UI)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)  # X11 only (XWayland works on Wayland desktops)
  endif()
  add_subdirectory(third_party/glfw EXCLUDE_FROM_ALL)
  find_package(OpenGL REQUIRED)
  add_subdirectory(ui)
endif()

# ---- copy runtime data next to the binaries
add_custom_target(copy_data ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/data
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets)


