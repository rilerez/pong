cmake_minimum_required(VERSION 3.14)
project(main CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/sdl2-cmake-modules)

if (NOT EMSCRIPTEN)
  find_package(SDL2 REQUIRED)
endif()

add_executable(main main.cpp)
set_property(TARGET main PROPERTY CXX_STANDARD 20)

function(target_compile_link_options)
  target_compile_options(${ARGV})
  target_link_options(${ARGV})
endfunction(target_compile_link_options)

target_compile_options(main PUBLIC "-O3")

if(EMSCRIPTEN)
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  target_compile_link_options(main PUBLIC "SHELL:-s USE_SDL=2")
  target_compile_link_options(main PUBLIC "SHELL:-s -fno-rtti")
else()

  target_link_libraries(main ${SDL2_LIBRARIES})
endif()
