LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/page/win"
    "${WEBCORE_DIR}/platform/graphics/wince"
    "${WEBCORE_DIR}/platform/graphics/win"
    "${WEBCORE_DIR}/platform/network/win"
    "${WEBCORE_DIR}/platform/win"

    wince
    wince/WebCoreSupport
)

LIST(APPEND WebKit_SOURCES
    wince/WebView.h
    wince/WebView.cpp

    wince/WebCoreSupport/ChromeClientWinCE.cpp
    wince/WebCoreSupport/ContextMenuClientWinCE.cpp
    wince/WebCoreSupport/DragClientWinCE.cpp
    wince/WebCoreSupport/EditorClientWinCE.cpp
    wince/WebCoreSupport/FrameLoaderClientWinCE.cpp
    wince/WebCoreSupport/FrameNetworkingContextWinCE.cpp
    wince/WebCoreSupport/InspectorClientWinCE.cpp
    wince/WebCoreSupport/PlatformStrategiesWinCE.cpp
)

SET(WebKit_LIBRARY_TYPE STATIC)
