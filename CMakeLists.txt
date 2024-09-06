cmake_minimum_required(VERSION 3.16)

project(led-backlight LANGUAGES C)

include(pigpio/util/Findpigpio.cmake)

#add_definitions(-DDEBUG) #DANGEROUS! IT WILL SKIP SECURITY CHECKS
add_executable(led-backlight main.c
  server/server.h
  server/server.c
  utils/utils.h
  utils/utils.c
  parser/parser.h
  parser/parser.c
  rgb/gpio.h
  rgb/gpio.c
  globals/globals.h
  globals/globals.c)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(led-backlight OpenSSL::SSL ${pigpiod_if2_LIBRARY} config ${CMAKE_THREAD_LIBS_INIT})

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
