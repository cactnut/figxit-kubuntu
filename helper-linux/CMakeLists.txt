cmake_minimum_required(VERSION 3.22)
project(figxit-helper-linux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Network DBus Test)
# Puts the list on KDE Plasma and the other Wayland desktops with layer-shell.
find_package(LayerShellQt QUIET)

add_executable(figxit-helper
  src/main.cpp
  src/protocol.cpp
  src/server.cpp
  src/popup.cpp
  src/placement.cpp
  src/accessibility.cpp
  src/engine.cpp
  src/theme.cpp
  src/backends.cpp
  src/locator.h
  src/locator/kwin.cpp
  src/resources.qrc
)
set_target_properties(figxit-helper PROPERTIES AUTORCC ON)
target_compile_options(figxit-helper PRIVATE -Wall -Wextra)
target_link_libraries(figxit-helper PRIVATE Qt6::Gui Qt6::Network Qt6::DBus)
if(LayerShellQt_FOUND)
  target_sources(figxit-helper PRIVATE src/surface/layershell.cpp)
  target_compile_definitions(figxit-helper PRIVATE FIGXIT_LAYER_SHELL)
  target_link_libraries(figxit-helper PRIVATE LayerShellQt::Interface)
else()
  message(WARNING "LayerShellQt was not found (liblayershellqtinterface-dev): the helper builds, but cannot show the list on Wayland")
endif()

enable_testing()
add_executable(placement_test tests/placement_test.cpp src/placement.cpp src/protocol.cpp)
target_link_libraries(placement_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME placement COMMAND placement_test)
