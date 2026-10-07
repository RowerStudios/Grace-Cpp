#my first proper usage of cmake
cmake_minimum_required(VERSION 3.21)
project(grace_cpp LANGUAGES C CXX) # include both C and C++
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # compile commands for glad
set(CMAKE_CXX_STANDARD 20) # cpp version
set(CMAKE_CXX_STANDARD_REQUIRED ON) # must have the cxx version defined above. or else error.
set(CMAKE_AUTOMOC ON) # qt needs moc for Q_OBJECT classes

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
find_package(SDL3 REQUIRED) # audio only, the window is qt's job

add_subdirectory(third_party/SDL_mixer) # audio only, the window is qt's job

add_library(glad third_party/glad/src/gl.c) #adding opengl libraries
target_include_directories(glad PUBLIC third_party/glad/include) # the opengl include folder that contains opengl related things

add_executable(Grace
  cpp/main.cpp
  cpp/game_widget.cpp
  cpp/renderer.cpp
  cpp/app_settings.cpp
  cpp/sound.cpp
  built-in-guis/main_menu.cpp
  built-in-guis/debug_panel.cpp)

# listed so automoc finds the Q_OBJECT classes, the headers live in headers/ and
# automoc only scans next to the sources otherwise
target_sources(Grace PRIVATE
  headers/game_widget.h
  headers/built-in-guis/main_menu.h
  headers/built-in-guis/debug_panel.h)

target_include_directories(Grace PRIVATE headers headers/built-in-guis third_party/stb)
target_link_libraries(Grace PRIVATE Qt6::Widgets Qt6::OpenGLWidgets Qt6::OpenGL glad SDL3_mixer::SDL3_mixer SDL3::SDL3)