cmake_minimum_required(VERSION 3.16)

include(WebKitCommon)
include(target/TargetWTF)
include(target/TargetJavaScriptCore)
include(target/PAL)
include(target/WebCore)

set_property(DIRECTORY . PROPERTY FOLDER "WebKitLegacy")

set(WebKitLegacy_SOURCES
    Storage/InProcessIDBServer.cpp
    Storage/StorageAreaImpl.cpp
    Storage/StorageAreaSync.cpp
    Storage/StorageNamespaceImpl.cpp
    Storage/StorageSyncManager.cpp
    Storage/StorageThread.cpp
    Storage/StorageTracker.cpp
    Storage/WebDatabaseProvider.cpp
    Storage/WebStorageNamespaceProvider.cpp

    WebCoreSupport/NetworkStorageSessionMap.cpp
    WebCoreSupport/WebBroadcastChannelRegistry.cpp
    WebCoreSupport/WebResourceLoadScheduler.cpp
    WebCoreSupport/WebViewGroup.cpp
)

set(WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WEBKITLEGACY_DIR}/Storage"
    "${WEBKITLEGACY_DIR}/WebCoreSupport"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}"
)

set(WebKitLegacy_FRAMEWORKS
    JavaScriptCore
    PAL
    WTF
    WebCore
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebKit_FRAMEWORKS bmalloc)
endif ()

set(WebKitLegacy_INTERFACE_LIBRARIES WebKitLegacy)
set(WebKitLegacy_INTERFACE_INCLUDE_DIRECTORIES ${WebKitLegacy_FRAMEWORK_HEADERS_DIR})
set(WebKitLegacy_INTERFACE_DEPENDENCIES WebKitLegacy_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(WebKitLegacy)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (MSVC)
    target_precompile_headers(WebKitLegacy PRIVATE win/WebKitPrefix.h)
    list(APPEND WebKitLegacy_SOURCES win/WebKit.resources/WebKit.rc)
    add_definitions(/DWEBKIT_EXPORTS /DFRAMEWORK_NAME=WebKitLegacy)
endif ()

WEBKIT_FRAMEWORK(WebKitLegacy)

WEBKIT_COPY_FILES(WebKitLegacy_CopyHeaders
    DESTINATION ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy
    FILES ${WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS}
    FLATTENED
)

if (APPLE)
    set_target_properties(WebKitLegacy PROPERTIES LINK_FLAGS "-umbrella WebKit")
endif ()

if (MSVC AND NOT WTF_PLATFORM_WIN_CAIRO)
    add_custom_command(
        TARGET WebKitLegacy
        POST_BUILD
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/version-stamp.pl ${WebKitLegacy_DERIVED_SOURCES_DIR} $<TARGET_FILE:WebKitLegacy>
        VERBATIM)

    if (USE_CA)
        add_subdirectory(win/WebKitQuartzCoreAdditions)
    endif ()
endif ()
