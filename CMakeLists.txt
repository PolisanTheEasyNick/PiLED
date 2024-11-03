cmake_minimum_required(VERSION 3.16)

project(piled LANGUAGES C)

set(CMAKE_CXX_FLAGS "-Wno-dev")

option(WITH_HTML "Build with HTML support" OFF)
option(WITH_WS "Build with WebSocket support" OFF)

add_definitions(-DDEBUG) #DANGEROUS! IT WILL SKIP SECURITY CHECKS
if(DEBUG)
  message(WARNING "!!!  DEBUG MODE IS ON  !!! ALL SECURITY CHECKS ARE SKIPPED")
endif()

include(pigpio/util/Findpigpio.cmake) #sets pigpio_INCLUDE_DIRS and pigpiod_if2_LIBRARY if found in system
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(pigpio QUIET)

if(WITH_WS)
  find_package(libwebsockets QUIET)
  if(NOT libwebsockets_FOUND)
    message(WARNING "libwebsockets not found in system! Building without WebSocket support.")
  endif()
endif()

if(WITH_HTML)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
  if(MICROHTTPD_FOUND)
    include_directories(${MICROHTTPD_INCLUDE_DIRS})
  else()
    message(WARNING "microhttpd not found in system! Building without HTTP server support.")
  endif()
endif()

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
  server/ws.h server/ws.c server/http.h
  utils/utils.h utils/utils.c
  parser/parser.h parser/parser.c
  parser/config.h parser/config.c
  rgb/gpio.h rgb/gpio.c
  rgb/openrgb.h rgb/openrgb.c
  globals/globals.h globals/globals.c
)

target_link_libraries(piled OpenSSL::SSL config ${CMAKE_THREAD_LIBS_INIT} ${pigpiod_if2_LIBRARY})

if(libwebsockets_FOUND AND WITH_WS)
  add_definitions(-Dlibwebsockets_FOUND)
  target_link_libraries(piled websockets)
endif()

if(MICROHTTPD_FOUND AND WITH_HTML)
  add_definitions(-Dmicrohttpd_FOUND)
  target_link_libraries(piled microhttpd)
endif()

if(NOT pigpio_FOUND)
  add_dependencies(piled pigpiod_if2)
endif()

add_executable(openrgb_configurator
  rgb/openrgb_configurator.c
  rgb/openrgb.h rgb/openrgb.c
  globals/globals.h globals/globals.c
  parser/config.h parser/config.c
  utils/utils.h utils/utils.c
)
target_link_libraries(openrgb_configurator config ${CMAKE_THREAD_LIBS_INIT})
target_compile_definitions(openrgb_configurator PRIVATE ORGBCONFIGURATOR)

include(GNUInstallDirs)
install(TARGETS piled
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(EXISTS "/etc/systemd/system")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/piled.service
    DESTINATION /etc/systemd/system)
endif()

install(DIRECTORY DESTINATION /etc/piled
  DIRECTORY_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/piled.conf
  DESTINATION /etc/piled/)
