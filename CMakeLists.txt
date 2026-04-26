cmake_minimum_required(VERSION 3.10)
project(fileParser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.3/mingw_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent CorePrivate)

qt_add_executable(fileParser
    src/main.cpp
    src/clientwindow.cpp
    include/clientwindow.h
    ui/clientwindow.ui
    src/server.cpp
    include/server.h
    src/client.cpp
    include/client.h
    src/launchwindow.cpp
    include/launchwindow.h
    src/serverwindow.cpp
    include/serverwindow.h

)

target_link_libraries(fileParser PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::CorePrivate)

target_include_directories(fileParser PRIVATE include src)


set_target_properties(fileParser PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/ui"
)

add_custom_command(TARGET fileParser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Program Files/OpenSSL-Win64/bin/libssl-4-x64.dll"
        "C:/Program Files/OpenSSL-Win64/bin/libcrypto-4-x64.dll"
        $<TARGET_FILE_DIR:fileParser>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/certs/server.crt"
        "${CMAKE_SOURCE_DIR}/certs/server.key"
        $<TARGET_FILE_DIR:fileParser>
)