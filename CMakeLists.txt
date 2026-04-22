cmake_minimum_required(VERSION 3.10)
project(fileParser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.3/mingw_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(fileParser
    src/main.cpp
    src/mainwindow.cpp
    include/mainwindow.h 
    ui/mainwindow.ui
)

target_link_libraries(fileParser PRIVATE Qt6::Widgets)
target_include_directories(fileParser PRIVATE include src)

set_target_properties(fileParser PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/ui"
)