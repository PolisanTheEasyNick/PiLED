cmake_minimum_required(VERSION 3.16)

project(led-backlight LANGUAGES C)

include(pigpio/util/Findpigpio.cmake)

add_executable(led-backlight main.c
    server.h
    server.c
    utils.h
    utils.c
    parser.h
    parser.c
    gpio.h
    gpio.c)


find_package(OpenSSL REQUIRED)
target_link_libraries(led-backlight OpenSSL::SSL ${pigpiod_if2_LIBRARY})

find_package(pigpio QUIET)
if(NOT pigpio_FOUND)
  add_subdirectory(pigpio)
  set(pigpio_INCLUDE_DIR pigpio)
  set(pigpiod_if2_LIBRARY pigpiod_if2)
endif()

include(GNUInstallDirs)
install(TARGETS led-backlight
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)