cmake_minimum_required(VERSION 3.12)
include(GNUInstallDirs)

project("Vision System"
  VERSION 0.0.1
  HOMEPAGE_URL https://github.com/ktchen14/overseer
  LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libudev REQUIRED IMPORTED_TARGET libudev)
pkg_check_modules(libvirt REQUIRED IMPORTED_TARGET libvirt)
pkg_check_modules(libxml2 REQUIRED IMPORTED_TARGET libxml-2.0)
pkg_check_modules(systemd REQUIRED IMPORTED_TARGET libsystemd)

add_executable(daemon device.c layout.c main.c)
set_target_properties(daemon PROPERTIES OUTPUT_NAME visiond)
target_compile_options(daemon PRIVATE -Wall -Wextra)
target_link_libraries(daemon PUBLIC
  PkgConfig::libudev
  PkgConfig::libvirt
  PkgConfig::libxml2
  PkgConfig::systemd)

install(TARGETS daemon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
