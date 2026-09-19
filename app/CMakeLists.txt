cmake_minimum_required(VERSION 3.16)

project(attackshark-ultra VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET libudev)

qt_standard_project_setup()

qt_add_executable(attackshark-ultra
    main.cpp
    mainwindow.cpp mainwindow.h
    ultra.cpp ultra.h
    theme.cpp theme.h
    widgets.cpp widgets.h
    dpibar.cpp dpibar.h
    mousediagram.cpp mousediagram.h
)

target_link_libraries(attackshark-ultra PRIVATE Qt6::Widgets PkgConfig::UDEV)

include(GNUInstallDirs)
install(TARGETS attackshark-ultra RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../udev/70-attack-shark-x11-ultra.rules
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d)
