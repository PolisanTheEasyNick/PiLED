cmake_minimum_required(VERSION 3.16)

project(led-backlight LANGUAGES C)

set(CMAKE_CXX_FLAGS "-Wno-dev")

#add_definitions(-DDEBUG) #DANGEROUS! IT WILL SKIP SECURITY CHECKS

include(pigpio/util/Findpigpio.cmake) #sets pigpio_INCLUDE_DIRS if found in system
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(pigpio QUIET)

if(NOT pigpio_FOUND)
  message("pigpio not found in system! Building from submodule instead.")
  add_subdirectory(pigpio)
  set(pigpio_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/pigpio")
  set(PIGPIOD_IF2_LIBRARY "${CMAKE_BINARY_DIR}/pigpio/libpigpiod_if2.so")
  include_directories(${pigpio_INCLUDE_DIRS})
endif()

add_executable(led-backlight
  main.c
  server/server.h server/server.c
  utils/utils.h utils/utils.c
  parser/parser.h parser/parser.c
  rgb/gpio.h rgb/gpio.c
  globals/globals.h globals/globals.c
)

target_link_libraries(led-backlight OpenSSL::SSL config ${CMAKE_THREAD_LIBS_INIT})

if(NOT pigpio_FOUND)
  target_link_libraries(led-backlight ${PIGPIOD_IF2_LIBRARY})
endif()

include(GNUInstallDirs)
install(TARGETS led-backlight
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

