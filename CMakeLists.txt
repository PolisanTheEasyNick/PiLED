cmake_minimum_required(VERSION 3.16)

project(led-backlight LANGUAGES C)

add_executable(led-backlight main.c
    server.h
    server.c
    utils.c
    utils.h)

include(GNUInstallDirs)
install(TARGETS led-backlight
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
