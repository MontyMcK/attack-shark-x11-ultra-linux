cmake_minimum_required(VERSION 3.16)

project(attackshark-ultra VERSION 0.1.0 LANGUAGES CXX)

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

# The window icon is compiled in so it shows up even before anything is
# installed; the hicolor copies below are what the desktop entry points at.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.png)
    qt_add_resources(attackshark-ultra "appicon"
        PREFIX "/"
        BASE ${CMAKE_CURRENT_SOURCE_DIR}/resources
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.png)
    target_compile_definitions(attackshark-ultra PRIVATE HAVE_APPICON)
endif()

target_link_libraries(attackshark-ultra PRIVATE Qt6::Widgets PkgConfig::UDEV)

include(GNUInstallDirs)

install(TARGETS attackshark-ultra RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Bake the full binary path into Exec. A bare command only works when the
# prefix happens to be on the desktop session's PATH, which ~/.local usually
# is not, and a launcher entry that silently does nothing is no fun to debug.
set(BIN "${CMAKE_INSTALL_FULL_BINDIR}/attackshark-ultra")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../packaging/attackshark-ultra.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/attackshark-ultra.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/attackshark-ultra.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

foreach(sz 16 24 32 48 64 128 256 512)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${sz}/attackshark-ultra.png)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${sz}/attackshark-ultra.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${sz}x${sz}/apps)
    endif()
endforeach()

# Only meaningful for a system prefix; a user install cannot load udev rules
# anyway, so do not litter ~/.local with one.
if(CMAKE_INSTALL_PREFIX MATCHES "^/usr")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../udev/70-attack-shark-x11-ultra.rules
            DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d)
endif()
