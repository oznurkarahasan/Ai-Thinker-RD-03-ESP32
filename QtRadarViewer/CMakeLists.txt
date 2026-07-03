cmake_minimum_required(VERSION 3.16)
project(RD03DRadarViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

add_executable(RD03DRadarViewer
    src/main.cpp
    src/SerialManager.cpp
    src/RadarProtocol.cpp
    src/RadarModel.cpp
    src/RadarView.cpp
    src/ControlPanel.cpp
    src/StatusPanel.cpp
    src/LogPanel.cpp
    src/MainWindow.cpp
    include/Types.h
    include/SerialManager.h
    include/RadarProtocol.h
    include/RadarModel.h
    include/RadarView.h
    include/ControlPanel.h
    include/StatusPanel.h
    include/LogPanel.h
    include/MainWindow.h
)

target_include_directories(RD03DRadarViewer PRIVATE include)
target_link_libraries(RD03DRadarViewer PRIVATE Qt6::Widgets Qt6::SerialPort)

set_target_properties(RD03DRadarViewer PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

if(RD03D_BUILD_SMOKE_TEST)
    add_executable(protocol_smoke_test
        src/protocol_smoke_test.cpp
        src/RadarProtocol.cpp
        src/RadarModel.cpp
        include/RadarProtocol.h
        include/RadarModel.h
        include/Types.h
    )
    target_include_directories(protocol_smoke_test PRIVATE include)
    target_link_libraries(protocol_smoke_test PRIVATE Qt6::Widgets)
endif()
