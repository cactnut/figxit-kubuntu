cmake_minimum_required(VERSION 3.22)
project(figxit-helper-linux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Network)
find_package(LayerShellQt REQUIRED)

add_executable(figxit-helper
  src/main.cpp
  src/protocol.cpp
  src/server.cpp
  src/popup.cpp
  src/theme.cpp
  src/surface/layershell.cpp
)
target_compile_options(figxit-helper PRIVATE -Wall -Wextra)
target_link_libraries(figxit-helper PRIVATE Qt6::Gui Qt6::Network LayerShellQt::Interface)
