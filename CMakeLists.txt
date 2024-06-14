cmake_minimum_required(VERSION 3.15)
project(amgame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

include(FetchContent)

FetchContent_Declare(
  box2d GIT_REPOSITORY https://github.com/erincatto/box2d.git GIT_TAG v2.4.1
  FIND_PACKAGE_ARGS
)

FetchContent_Declare(
  sockpp GIT_REPOSITORY https://github.com/fpagliughi/sockpp GIT_TAG v1.0.0
  FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(box2d sockpp)

set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)

set(SOCKPP_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(SOCKPP_BUILD_SHARED OFF CACHE BOOL "" FORCE)

# Surpress warnings from 3rd party libraries
target_compile_options(sockpp-objs PRIVATE -Wno-unused -Wno-unused-parameter -Wno-sign-compare)
target_compile_options(box2d PRIVATE -w)

add_executable(
  mniam_headless
  src/amcom.c
  src/amgame.cpp
  src/engine/engine.cpp
  src/food.cpp
  src/main.cpp
  src/player.cpp
  src/remote_connection.cpp
  src/connection_server.cpp
  src/connection_client.cpp
)
target_include_directories(mniam_headless PRIVATE src/engine ${box2d_SOURCE_DIR}/include/box2d)
target_link_libraries(mniam_headless box2d sockpp-static)
