cmake_minimum_required(VERSION 2.8.12)
include(WebKitCommon)

set(WebKit_SOURCES
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
endif ()

add_library(WebKit ${WebKit_LIBRARY_TYPE} ${WebKit_SOURCES})
add_dependencies(WebKit WebCore)
target_link_libraries(WebKit ${WebKit_LIBRARIES})
set_target_properties(WebKit PROPERTIES FOLDER "WebKit")

if (WebKit_OUTPUT_NAME)
    set_target_properties(WebKit PROPERTIES OUTPUT_NAME ${WebKit_OUTPUT_NAME})
endif ()

POPULATE_LIBRARY_VERSION(WEBKIT)
set_target_properties(WebKit PROPERTIES VERSION ${WEBKIT_VERSION} SOVERSION ${WEBKIT_VERSION_MAJOR})
install(TARGETS WebKit DESTINATION "${LIB_INSTALL_DIR}")
