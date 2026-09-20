cmake_minimum_required(VERSION 3.21)

project(lingwtools VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MINGW)
  add_compile_options(-Wa,-mbig-obj)
  # windres 按 UTF-8 解析 .rc，修复中文版本信息乱码
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -c 65001")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Svg Concurrent)

# ---------------- third party ----------------
# yaml-cpp (static)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/yaml-cpp)

# libqrencode core sources (no config.h needed)
add_library(qrencode_core STATIC
  3rdparty/qrencode/qrencode.c
  3rdparty/qrencode/qrinput.c
  3rdparty/qrencode/qrspec.c
  3rdparty/qrencode/rsecc.c
  3rdparty/qrencode/split.c
  3rdparty/qrencode/mask.c
  3rdparty/qrencode/mmask.c
  3rdparty/qrencode/mqrspec.c
  3rdparty/qrencode/bitstream.c
)
target_include_directories(qrencode_core PUBLIC 3rdparty/qrencode)
target_compile_definitions(qrencode_core PRIVATE
  STATIC_IN_RELEASE=static
  MAJOR_VERSION=4
  MINOR_VERSION=1
  MICRO_VERSION=1
  VERSION="4.1.1"
)
if(MSVC)
  target_compile_definitions(qrencode_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# mbedtls (static, crypto only)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/mbedtls)

# ---------------- app sources ----------------
set(APP_SOURCES
  src/main.cpp
  src/mainwindow.cpp
  src/registry.cpp
  src/iconprovider.cpp
  src/widgets/toolpage.cpp
  src/widgets/flowlayout.cpp
  src/widgets/collapsiblesection.cpp
  src/widgets/highlighters.cpp
  src/core/codecs.cpp
  src/core/texttools.cpp
  src/core/nettools.cpp
  src/core/icmp.cpp
  src/core/crypto.cpp
  src/core/sysinfo.cpp
  src/tools/codectoolspages.cpp
  src/tools/texttoolspages.cpp
  src/tools/nettoolspages.cpp
  src/tools/cryptoolspages.cpp
  src/tools/k8stoolspages.cpp
  src/tools/systoolspages.cpp
)

if(WIN32)
  list(APPEND APP_SOURCES resources/app.rc)
endif()

add_executable(lingwtools WIN32 ${APP_SOURCES} theme.qrc)

target_include_directories(lingwtools PRIVATE src 3rdparty/mbedtls/include)

target_link_libraries(lingwtools PRIVATE
  Qt6::Widgets Qt6::Network Qt6::Svg Qt6::Concurrent
  yaml-cpp qrencode_core
  mbedcrypto
)

if(WIN32)
  target_link_libraries(lingwtools PRIVATE iphlpapi ws2_32 bcrypt)
endif()
if(APPLE)
  target_link_libraries(lingwtools PRIVATE "-framework Security" "-framework CoreFoundation")
endif()

set_target_properties(lingwtools PROPERTIES
  OUTPUT_NAME "lingwtools"
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "灵王工具箱"
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.lingwang.toolbox"
  MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)
