cmake_minimum_required(VERSION 3.16)

project(piled LANGUAGES C)

set(CMAKE_CXX_FLAGS "-Wno-dev")

#add_definitions(-DDEBUG) #DANGEROUS! IT WILL SKIP SECURITY CHECKS

include(pigpio/util/Findpigpio.cmake) #sets pigpio_INCLUDE_DIRS and pigpiod_if2_LIBRARY if found in system
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(pigpio QUIET)

if(NOT pigpio_FOUND)
  message(WARNING "pigpio not found in system! Will be builded from submodule.")
  add_subdirectory("${CMAKE_SOURCE_DIR}/pigpio")
  set(pigpio_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/pigpio")
  set(pigpiod_if2_LIBRARY "${CMAKE_BINARY_DIR}/pigpio/libpigpiod_if2.so")
endif()

include_directories(${pigpio_INCLUDE_DIRS})

add_executable(piled
  main.c
  server/server.h server/server.c
  utils/utils.h utils/utils.c
  parser/parser.h parser/parser.c
  rgb/gpio.h rgb/gpio.c
  globals/globals.h globals/globals.c
)

target_link_libraries(piled OpenSSL::SSL config ${CMAKE_THREAD_LIBS_INIT} ${pigpiod_if2_LIBRARY})
if(NOT pigpio_FOUND)
  add_dependencies(piled pigpiod_if2) #is there cleaner way to make submodule build??? 
endif()

include(GNUInstallDirs)
install(TARGETS piled
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(EXISTS "/etc/systemd/system")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/piled.service
    DESTINATION /etc/systemd/system)
endif()


install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/piled.conf
  DESTINATION /etc/config)
