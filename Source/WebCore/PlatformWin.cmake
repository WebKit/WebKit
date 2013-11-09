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

    html/HTMLSelectElementWin.cpp

    page/win/DragControllerWin.cpp
    page/win/EventHandlerWin.cpp
    page/win/FrameWin.cpp

    platform/Cursor.cpp
    platform/LocalizedStrings.cpp
    platform/PlatformStrategies.cpp

    platform/graphics/opentype/OpenTypeUtilities.cpp

    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/GDIExtras.cpp
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

    plugins/PluginDatabase.cpp
    plugins/PluginPackage.cpp

    plugins/win/PluginDatabaseWin.cpp
)

if (ENABLE_NETSCAPE_PLUGIN_API)
    list(APPEND WebCore_SOURCES
        plugins/PluginView.cpp

        plugins/win/PluginMessageThrottlerWin.cpp
        plugins/win/PluginPackageWin.cpp
        plugins/win/PluginViewWin.cpp
    )
else ()
    list(APPEND WebCore_SOURCES
        plugins/PluginPackageNone.cpp
        plugins/PluginViewNone.cpp
    )
endif ()
