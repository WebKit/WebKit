
project( WebKit )

# search packages used by KDE
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)

find_package(LibXml2 REQUIRED)
find_package(LibXslt REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG  REQUIRED)

find_program(BISON_EXECUTABLE bison)
find_program(GPERF_EXECUTABLE gperf)

# quick check the libicu is available
find_path(LIBICU_INCLUDE_DIR unicode/umachine.h )

find_path(LIBCURL_INCLUDE_DIR curl/curl.h )

if (NOT BISON_EXECUTABLE)
   message(FATAL_ERROR "Could not find bison")
endif (NOT BISON_EXECUTABLE)

if (NOT GPERF_EXECUTABLE)
   message(FATAL_ERROR "Could not find gperf")
endif (NOT GPERF_EXECUTABLE)

if (NOT LIBICU_INCLUDE_DIR)
   message(FATAL_ERROR "libicu not found, cannot create unity without it")
endif (NOT LIBICU_INCLUDE_DIR)

if (NOT LIBCURL_INCLUDE_DIR)
   message(FATAL_ERROR "libcurl not found, cannot create unity without it")
endif (NOT LIBCURL_INCLUDE_DIR)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -DQT3_SUPPORT -DHAVE_CONFIG_H=1)
link_directories (${KDE4_LIB_DIR})
include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${LIBICU_INCLUDE_DIR})

add_definitions( -fpermissive )
add_definitions( -DUSE_SYSTEM_MALLOC=1 )

# Once we use KDE stuff, adjust wtf/Platform.h for BUILDING_KDE__ ...
add_definitions( -DBUILDING_QT__=1 )

add_subdirectory( JavaScriptCore ) 
add_subdirectory( WebCore ) 
add_subdirectory( WebKitTools/QtLauncher )

include_directories ( BEFORE ${CMAKE_CURRENT_BINARY_DIR} )

