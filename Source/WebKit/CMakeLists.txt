cmake_minimum_required(VERSION 2.8.12)
include(WebKitCommon)

set(WebKit_SOURCES
    WebCoreSupport/WebResourceLoadScheduler.cpp
    WebCoreSupport/WebViewGroup.cpp
)

set(WebKit_INCLUDE_DIRECTORIES
    WebCoreSupport
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}/Source"
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebCore"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKitLegacy"
)

set(WebKit_SYSTEM_INCLUDE_DIRECTORIES "")

set(WebKit_LIBRARIES
    PRIVATE JavaScriptCore${DEBUG_SUFFIX}
    PRIVATE WTF${DEBUG_SUFFIX}
    PRIVATE WebCore${DEBUG_SUFFIX}
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(${WebKit_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${WebKit_SYSTEM_INCLUDE_DIRECTORIES})

if (MSVC)
    ADD_PRECOMPILED_HEADER("WebKitPrefix.h" "win/WebKitPrefix.cpp" WebKit_SOURCES)
    list(APPEND WebKit_SOURCES win/WebKit.resources/WebKit.rc)
    add_definitions(/DWEBKIT_EXPORTS /DFRAMEWORK_NAME=WebKit)
endif ()

add_library(WebKit ${WebKit_LIBRARY_TYPE} ${WebKit_SOURCES})
add_dependencies(WebKit WebCore)
if (WIN32)
    add_dependencies(WebKit WebKitGUID)
endif ()
target_link_libraries(WebKit ${WebKit_LIBRARIES})
set_target_properties(WebKit PROPERTIES FOLDER "WebKit")

if (WebKit_OUTPUT_NAME)
    set_target_properties(WebKit PROPERTIES OUTPUT_NAME ${WebKit_OUTPUT_NAME})
endif ()

WEBKIT_FRAMEWORK(WebKit)
