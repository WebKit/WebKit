project (WebKit)

# Qt support is always enabled!
add_definitions (-DBUILDING_QT__=1)

# Global switch for KDE support
OPTION (WEBKIT_USE_KDE_SUPPORT "Compile WebKit with KDE support." ON)

IF (WEBKIT_USE_KDE_SUPPORT)
    add_definitions (-DBUILDING_KDE__=1)
ENDIF (WEBKIT_USE_KDE_SUPPORT)

# Global switch for SVG support
OPTION (WEBKIT_USE_SVG_SUPPORT "Compile WebKit with SVG support." ON)

IF (WEBKIT_USE_SVG_SUPPORT)
    add_definitions (-DSVG_SUPPORT=1)
ENDIF (WEBKIT_USE_SVG_SUPPORT)

# Global switch for XPath support
OPTION (WEBKIT_USE_XPATH_SUPPORT "Compile WebKit with XPath support." ON)

IF (WEBKIT_USE_XPATH_SUPPORT)
    add_definitions (-DXPATH_SUPPORT=1)
ENDIF (WEBKIT_USE_XPATH_SUPPORT)

# Global switch for XSLT support
OPTION (WEBKIT_USE_XSLT_SUPPORT "Compile WebKit with XSLT support." ON)

IF (WEBKIT_USE_XSLT_SUPPORT)
    add_definitions (-DXSLT_SUPPORT=1)
ENDIF (WEBKIT_USE_XSLT_SUPPORT)

# Global switch for XBL support
OPTION (WEBKIT_USE_XBL_SUPPORT "Compile WebKit with XBL support." OFF)

IF (WEBKIT_USE_XBL_SUPPORT)
    add_definitions (-DXBL_SUPPORT=1)
ENDIF (WEBKIT_USE_XBL_SUPPORT)

# search packages used by KDE
IF (WEBKIT_USE_KDE_SUPPORT)
    find_package (KDE4 REQUIRED)
    include (KDE4Defaults)
    include (MacroLibrary)

    # use the KDE modules for finding these
    find_package (LibXml2 REQUIRED)
    find_package (LibXslt REQUIRED)

ELSE (WEBKIT_USE_KDE_SUPPORT)
    INCLUDE(UsePkgConfig)

    PKGCONFIG(libxml-2.0 _LibXml2IncDir _LibXml2LinkDir _LibXml2LinkFlags _LibXml2CFlags)
    SET(LIBXML2_LIBRARIES ${_LibXml2LinkFlags})
    # I'd like to use the output of --cflags directly but cmake craps out on it ;(
    SET(LIBXML2_INCLUDE_DIR ${_LibXml2IncDir}/libxml2)

    PKGCONFIG(libxslt _LibXslt2IncDir _LibXslt2LinkDir _LibXslt2LinkFlags _LibXslt2CFlags)
    SET(LIBXSLT_LIBRARIES ${_LibXslt2LinkFlags})
    SET(LIBXSLT_INCLUDE_DIR ${_LibXslt2IncDir})

    include(CheckLibraryExists)
    include(FindQt4)
ENDIF (WEBKIT_USE_KDE_SUPPORT)

# Special option for BuildBot, to disable output colorization (must be done after KDE4Defaults is loaded)
OPTION (WEBKIT_DO_NOT_USE_COLORFUL_OUTPUT "Do not colorize compilation output." OFF)

IF (WEBKIT_DO_NOT_USE_COLORFUL_OUTPUT)
    set(CMAKE_COLOR_MAKEFILE OFF)
ENDIF (WEBKIT_DO_NOT_USE_COLORFUL_OUTPUT)

find_package (JPEG REQUIRED)
find_package (PNG REQUIRED)

find_program (BISON_EXECUTABLE bison)
find_program (GPERF_EXECUTABLE gperf)

# quick check the libicu is available
find_path(LIBICU_INCLUDE_DIR unicode/umachine.h )

if (NOT BISON_EXECUTABLE)
   message(FATAL_ERROR "Could not find bison")
endif (NOT BISON_EXECUTABLE)

if (NOT GPERF_EXECUTABLE)
   message(FATAL_ERROR "Could not find gperf")
endif (NOT GPERF_EXECUTABLE)

if (NOT LIBICU_INCLUDE_DIR)
   message(FATAL_ERROR "libicu not found, cannot create unity without it")
endif (NOT LIBICU_INCLUDE_DIR)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -DQT3_SUPPORT -DHAVE_CONFIG_H=1)
link_directories (${KDE4_LIB_DIR})
include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${LIBICU_INCLUDE_DIR})

add_definitions (-fpermissive)
add_definitions (-DUSE_SYSTEM_MALLOC=1)

add_subdirectory (JavaScriptCore)
add_subdirectory (WebCore)
IF (WEBKIT_USE_KDE_SUPPORT)
    add_subdirectory (WebKitTools/DumpRenderTree/DumpRenderTree.qtproj)
ENDIF (WEBKIT_USE_KDE_SUPPORT)

add_subdirectory (WebKitQt/QtLauncher)

IF (WEBKIT_USE_KDE_SUPPORT)
    add_subdirectory (WebKitQt/WebKitPart)
ENDIF (WEBKIT_USE_KDE_SUPPORT)

include_directories (BEFORE ${CMAKE_CURRENT_BINARY_DIR})
