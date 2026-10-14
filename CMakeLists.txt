cmake_minimum_required(VERSION 3.16)

project(ds_uds_server VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_QT_MONITOR "Build the Qt CAN/UDS monitor" OFF)
option(BUILD_TESTING "Build unit tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

configure_file(cmake/version.h.in generated/uds/version.h @ONLY)

add_library(uds_core
    server/src/uds_message.cpp
    server/src/can_socket.cpp
    server/src/session_manager.cpp
    server/src/security_manager.cpp
    server/src/data_manager.cpp
    server/src/routine_manager.cpp
    server/src/dtc_manager.cpp
    server/src/uds_protocol.cpp
    server/src/uds_server.cpp
)
target_include_directories(uds_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/server/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)
target_compile_options(uds_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
)

add_executable(uds_server server/src/main.cpp)
target_link_libraries(uds_server PRIVATE uds_core)

if(BUILD_QT_MONITOR)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOUIC_SEARCH_PATHS monitor/ui)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets SerialBus)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets SerialBus)
    add_executable(can_monitor
        monitor/main.cpp
        monitor/mainwindow.cpp monitor/mainwindow.h monitor/ui/mainwindow.ui
        monitor/canconnection.cpp monitor/canconnection.h
        monitor/canframemodel.cpp monitor/canframemodel.h
        monitor/filterproxymodel.cpp monitor/filterproxymodel.h
        monitor/framedelegate.cpp monitor/framedelegate.h
        monitor/hexutil.cpp monitor/hexutil.h
        monitor/isotpreassembler.cpp monitor/isotpreassembler.h
        monitor/settingsdialog.cpp monitor/settingsdialog.h
        monitor/trafficstatistics.cpp monitor/trafficstatistics.h
        monitor/udsclient.cpp monitor/udsclient.h
        monitor/udsdecoder.cpp monitor/udsdecoder.h
    )
    target_link_libraries(can_monitor PRIVATE
        Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::SerialBus
    )
endif()

if(BUILD_TESTING)
    enable_testing()
    add_executable(uds_core_tests tests/uds_core_tests.cpp)
    target_link_libraries(uds_core_tests PRIVATE uds_core)
    add_test(NAME uds_core_tests COMMAND uds_core_tests)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME uds_python_tests
                 COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_uds_client.py)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS uds_server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY server/include/uds DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
