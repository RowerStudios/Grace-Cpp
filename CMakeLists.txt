#my first proper usage of cmake
cmake_minimum_required(VERSION 3.20) #self explanatory
project(grace_cpp LANGUAGES C CXX) # include both C and C++ 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # compile commands for glad
set(CMAKE_CXX_STANDARD 20) # cpp version
set(CMAKE_CXX_STANDARD_REQUIRED ON) # must have the cxx version defined above. or else error.
add_subdirectory(third_party/SDL3) #add sdl3
add_library(glad third_party/glad/src/gl.c) #adding opengl libraries
target_include_directories(glad PUBLIC third_party/glad/include) # the opengl include folder that contains opengl related things
add_executable(Grace game.cpp renderer.cpp) # name/cpp/../.....
target_include_directories(Grace PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # include the dot directory
target_link_libraries(Grace PRIVATE SDL3::SDL3 glad) # include SDL3 shits