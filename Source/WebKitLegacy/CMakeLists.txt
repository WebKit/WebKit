cmake_minimum_required(VERSION 2.8.12)
include(WebKitCommon)
set_property(DIRECTORY . PROPERTY FOLDER "WebKitLegacy")

set(WebKitLegacy_SOURCES
    Storage/StorageAreaImpl.cpp
    Storage/StorageAreaSync.cpp
    Storage/StorageNamespaceImpl.cpp
    Storage/StorageSyncManager.cpp
    Storage/StorageThread.cpp
    Storage/StorageTracker.cpp
    Storage/WebDatabaseProvider.cpp
    Storage/WebStorageNamespaceProvider.cpp

    WebCoreSupport/NetworkStorageSessionMap.cpp
    WebCoreSupport/WebResourceLoadScheduler.cpp
    WebCoreSupport/WebViewGroup.cpp
)

set(WebKitLegacy_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WEBKITLEGACY_DIR}/Storage"
    "${WEBKITLEGACY_DIR}/WebCoreSupport"
    "${FORWARDING_HEADERS_DIR}"
)

set(WebKitLegacy_SYSTEM_INCLUDE_DIRECTORIES
    "${ICU_INCLUDE_DIRS}"
)

set(WebKitLegacy_LIBRARIES
    PRIVATE JavaScriptCore${DEBUG_SUFFIX}
    PRIVATE WebCore${DEBUG_SUFFIX}
    PRIVATE PAL${DEBUG_SUFFIX}
)

WEBKIT_FRAMEWORK_DECLARE(WebKitLegacy)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (MSVC)
    WEBKIT_ADD_PRECOMPILED_HEADER("WebKitPrefix.h" "win/WebKitPrefix.cpp" WebKitLegacy_SOURCES)
    list(APPEND WebKitLegacy_SOURCES win/WebKit.resources/WebKit.rc)
    add_definitions(/DWEBKIT_EXPORTS /DFRAMEWORK_NAME=WebKitLegacy)
endif ()

WEBKIT_FRAMEWORK(WebKitLegacy)

add_dependencies(WebKitLegacy WebCore)
if (TARGET WebKitLegacyGUID)
    add_dependencies(WebKitLegacy WebKitLegacyGUID)
endif ()

if (APPLE)
    set_target_properties(WebKitLegacy PROPERTIES LINK_FLAGS "-umbrella WebKit")
endif ()

if (MSVC)
    add_custom_command(
        TARGET WebKitLegacy
        POST_BUILD
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/version-stamp.pl ${DERIVED_SOURCES_WEBKITLEGACY_DIR} $<TARGET_FILE:WebKitLegacy>
        VERBATIM)

     if (NOT WTF_PLATFORM_WIN_CAIRO)
         add_subdirectory(win/WebKitQuartzCoreAdditions)
     endif ()
endif ()
