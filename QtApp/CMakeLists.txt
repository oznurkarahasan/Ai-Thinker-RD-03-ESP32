cmake_minimum_required(VERSION 3.16)
project(RadarDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Release builds by default: this is a latency-sensitive real-time viewer.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

add_executable(RadarDesktop
    src/main.cpp
    src/RadarComm.cpp
    src/ZoneGridItem.cpp
    src/TargetItem.cpp
    src/RadarScene.cpp
    src/DashboardPanel.cpp
    src/MainWindow.cpp
    include/RadarTypes.h
    include/RadarComm.h
    include/ZoneGridItem.h
    include/TargetItem.h
    include/RadarScene.h
    include/DashboardPanel.h
    include/MainWindow.h
)

target_include_directories(RadarDesktop PRIVATE include)

target_link_libraries(RadarDesktop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
)

target_compile_options(RadarDesktop PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
)
