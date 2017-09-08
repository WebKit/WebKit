set(WebKit2_OUTPUT_NAME WebKit2)
set(WebKit2_WebProcess_OUTPUT_NAME WebKitWebProcess)
set(WebKit2_NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(WebKit2_PluginProcess_OUTPUT_NAME WebKitPluginProcess)
set(WebKit2_StorageProcess_OUTPUT_NAME WebKitStorageProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})

add_definitions(-DBUILDING_WEBKIT)

list(APPEND WebKit2_SOURCES
    NetworkProcess/Downloads/curl/DownloadCurl.cpp

    NetworkProcess/curl/NetworkProcessCurl.cpp
    NetworkProcess/curl/RemoteNetworkingContextCurl.cpp

    NetworkProcess/win/NetworkProcessMainWin.cpp
    NetworkProcess/win/SystemProxyWin.cpp

    StorageProcess/win/StorageProcessMainWin.cpp
)

# DerivedSources/JavaScriptCore/inspector/InspectorBackendCommands.js is
# expected in DerivedSources/WebInspectorUI/UserInterface/Protocol/.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    DEPENDS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js
    COMMAND cp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
)

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBKIT2_DIR}/NetworkProcess/win"
    "${WEBKIT2_DIR}/Platform/classifier"
    "${WEBKIT2_DIR}/PluginProcess/win"
    "${WEBKIT2_DIR}/Shared/API/c/win"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT2_DIR}/Shared/Plugins/win"
    "${WEBKIT2_DIR}/Shared/unix"
    "${WEBKIT2_DIR}/Shared/win"
    "${WEBKIT2_DIR}/StorageProcess/win"
    "${WEBKIT2_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT2_DIR}/UIProcess/API/C/win"
    "${WEBKIT2_DIR}/UIProcess/API/cpp/win"
    "${WEBKIT2_DIR}/UIProcess/API/win"
    "${WEBKIT2_DIR}/UIProcess/Plugins/win"
    "${WEBKIT2_DIR}/UIProcess/win"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/win"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/win/DOM"
    "${WEBKIT2_DIR}/WebProcess/win"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/win"
    "${WEBKIT2_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT2_DIR}/WebProcess/WebPage/win"
    "${WEBKIT2_DIR}/win"
)

list(APPEND WebKit2_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
)

set(WebKit2CommonIncludeDirectories ${WebKit2_INCLUDE_DIRECTORIES})
set(WebKit2CommonSystemIncludeDirectories ${WebKit2_SYSTEM_INCLUDE_DIRECTORIES})

list(APPEND WebProcess_SOURCES
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/win/NetworkProcessMain.cpp
)

list(APPEND StorageProcess_SOURCES
    StorageProcess/EntryPoint/win/StorageProcessMain.cpp
)

if (${ENABLE_PLUGIN_PROCESS})
    list(APPEND PluginProcess_SOURCES
    )
endif ()

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DUSE_CAIRO=1 -DUSE_CURL=1)

    list(APPEND WebKit2_LIBRARIES
        libeay32.lib
        mfuuid.lib
        ssleay32.lib
        strmiids.lib
    )
endif ()

list(APPEND WebKit2_LIBRARIES
    WebCoreDerivedSources${DEBUG_SUFFIX}
)

set(SharedWebKit2Libraries
    ${WebKit2_LIBRARIES}
)

add_custom_target(WebKit2-forwarding-headers
                  COMMAND ${PERL_EXECUTABLE}
                  ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl
                  --include-path ${WEBKIT2_DIR}
                  --output ${FORWARDING_HEADERS_DIR}
                  --platform win
                  --platform curl
                  )

set(WEBKIT2_EXTRA_DEPENDENCIES
    WebKit2-forwarding-headers
)

WEBKIT_WRAP_SOURCELIST(${WebKit2_SOURCES})
