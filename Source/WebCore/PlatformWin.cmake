list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/win"
    "${WEBCORE_DIR}/page/win"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/win"
    "${WEBCORE_DIR}/platform/win"
    "${WEBCORE_DIR}/plugins/win"
)

list(APPEND WebCore_SOURCES
    accessibility/win/AccessibilityObjectWin.cpp

    editing/win/EditorWin.cpp

    html/HTMLSelectElementWin.cpp

    page/win/DragControllerWin.cpp
    page/win/EventHandlerWin.cpp
    page/win/FrameWin.cpp

    platform/Cursor.cpp
    platform/LocalizedStrings.cpp
    platform/PlatformStrategies.cpp

    platform/graphics/opentype/OpenTypeUtilities.cpp

    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/IconWin.cpp
    platform/graphics/win/ImageWin.cpp
    platform/graphics/win/IntPointWin.cpp
    platform/graphics/win/IntRectWin.cpp
    platform/graphics/win/IntSizeWin.cpp

    platform/network/win/NetworkStateNotifierWin.cpp

    platform/win/BString.cpp
    platform/win/BitmapInfo.cpp
    platform/win/ClipboardUtilitiesWin.cpp
    platform/win/ContextMenuItemWin.cpp
    platform/win/ContextMenuWin.cpp
    platform/win/CursorWin.cpp
    platform/win/DefWndProcWindowClass.cpp
    platform/win/DragDataWin.cpp
    platform/win/DragImageWin.cpp
    platform/win/EventLoopWin.cpp
    platform/win/FileSystemWin.cpp
    platform/win/KeyEventWin.cpp
    platform/win/LanguageWin.cpp
    platform/win/LocalizedStringsWin.cpp
    platform/win/LoggingWin.cpp
    platform/win/MIMETypeRegistryWin.cpp
    platform/win/PasteboardWin.cpp
    platform/win/PlatformMouseEventWin.cpp
    platform/win/PlatformScreenWin.cpp
    platform/win/PopupMenuWin.cpp
    platform/win/SSLKeyGeneratorWin.cpp
    platform/win/ScrollbarThemeWin.cpp
    platform/win/SearchPopupMenuWin.cpp
    platform/win/SharedBufferWin.cpp
    platform/win/SharedTimerWin.cpp
    platform/win/SoundWin.cpp
    platform/win/SystemInfo.cpp
    platform/win/WCDataObject.cpp
    platform/win/WebCoreInstanceHandle.cpp
    platform/win/WheelEventWin.cpp
    platform/win/WidgetWin.cpp
)

list(APPEND WebCore_SOURCES
    "${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreHeaderDetection.h"
)

if (${WTF_PLATFORM_WIN_CAIRO})
    include(PlatformWinCairo.cmake)
endif ()

set(WebCore_FORWARDING_HEADERS_DIRECTORIES
    Modules/indexeddb

    bridge/c
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebCore DIRECTORIES ${WebCore_FORWARDING_HEADERS_DIRECTORIES})

# FIXME: This should test if AVF headers are available.
# https://bugs.webkit.org/show_bug.cgi?id=135861
add_custom_command(
    OUTPUT "${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreHeaderDetection.h"
    WORKING_DIRECTORY "${DERIVED_SOURCES_WEBCORE_DIR}"
    COMMAND echo /* Identifying AVFoundation Support */ > WebCoreHeaderDetection.h
    VERBATIM)
