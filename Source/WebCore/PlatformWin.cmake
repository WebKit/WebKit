add_definitions(/bigobj -D__STDC_CONSTANT_MACROS)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/../include/private"
    "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore"
    "${WEBCORE_DIR}/accessibility/win"
    "${WEBCORE_DIR}/page/win"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/win"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/network/win"
    "${WEBCORE_DIR}/platform/win"
)

list(APPEND WebCore_SOURCES
    accessibility/win/AXObjectCacheWin.cpp
    accessibility/win/AccessibilityObjectWin.cpp
    accessibility/win/AccessibilityObjectWrapperWin.cpp

    editing/win/EditorWin.cpp

    html/HTMLSelectElementWin.cpp

    page/win/DragControllerWin.cpp
    page/win/EventHandlerWin.cpp
    page/win/FrameWin.cpp

    platform/Cursor.cpp
    platform/LocalizedStrings.cpp
    platform/StaticPasteboard.cpp

    platform/audio/PlatformMediaSessionManager.cpp

    platform/graphics/egl/GLContextEGL.cpp

    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeUtilities.cpp

    platform/graphics/win/ComplexTextControllerUniscribe.cpp
    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/DisplayRefreshMonitorWin.cpp
    platform/graphics/win/FloatRectWin.cpp
    platform/graphics/win/FontCacheWin.cpp
    platform/graphics/win/FontDescriptionWin.cpp
    platform/graphics/win/FontPlatformDataWin.cpp
    platform/graphics/win/FontWin.cpp
    platform/graphics/win/FullScreenController.cpp
    platform/graphics/win/GraphicsContextWin.cpp
    platform/graphics/win/IconWin.cpp
    platform/graphics/win/ImageWin.cpp
    platform/graphics/win/IntPointWin.cpp
    platform/graphics/win/IntRectWin.cpp
    platform/graphics/win/IntSizeWin.cpp
    platform/graphics/win/MediaPlayerPrivateFullscreenWindow.cpp
    platform/graphics/win/SimpleFontDataWin.cpp
    platform/graphics/win/SystemFontDatabaseWin.cpp
    platform/graphics/win/TransformationMatrixWin.cpp

    platform/network/win/DownloadBundleWin.cpp
    platform/network/win/NetworkStateNotifierWin.cpp

    platform/win/BString.cpp
    platform/win/BitmapInfo.cpp
    platform/win/ClipboardUtilitiesWin.cpp
    platform/win/CursorWin.cpp
    platform/win/DefWndProcWindowClass.cpp
    platform/win/DragDataWin.cpp
    platform/win/DragImageWin.cpp
    platform/win/GDIObjectCounter.cpp
    platform/win/GDIUtilities.cpp
    platform/win/KeyEventWin.cpp
    platform/win/LocalizedStringsWin.cpp
    platform/win/LoggingWin.cpp
    platform/win/MIMETypeRegistryWin.cpp
    platform/win/MainThreadSharedTimerWin.cpp
    platform/win/PasteboardWin.cpp
    platform/win/PlatformMouseEventWin.cpp
    platform/win/PlatformScreenWin.cpp
    platform/win/PopupMenuWin.cpp
    platform/win/SSLKeyGeneratorWin.cpp
    platform/win/ScrollbarThemeWin.cpp
    platform/win/SearchPopupMenuDB.cpp
    platform/win/SearchPopupMenuWin.cpp
    platform/win/SystemInfo.cpp
    platform/win/UserAgentWin.cpp
    platform/win/WCDataObject.cpp
    platform/win/WebCoreBundleWin.cpp
    platform/win/WebCoreInstanceHandle.cpp
    platform/win/WebCoreTextRenderer.cpp
    platform/win/WheelEventWin.cpp
    platform/win/WidgetWin.cpp
    platform/win/WindowMessageBroadcaster.cpp
    platform/win/WindowsKeyNames.cpp

    rendering/RenderThemeWin.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/win/AccessibilityObjectWrapperWin.h

    page/win/FrameWin.h

    platform/graphics/opentype/FontMemoryResource.h

    platform/graphics/win/DIBPixelData.h
    platform/graphics/win/FontCustomPlatformData.h
    platform/graphics/win/FullScreenController.h
    platform/graphics/win/FullScreenControllerClient.h
    platform/graphics/win/GraphicsContextWin.h
    platform/graphics/win/LocalWindowsContext.h
    platform/graphics/win/MediaPlayerPrivateFullscreenWindow.h
    platform/graphics/win/SharedGDIObject.h

    platform/win/BString.h
    platform/win/BitmapInfo.h
    platform/win/COMPtr.h
    platform/win/DefWndProcWindowClass.h
    platform/win/GDIObjectCounter.h
    platform/win/GDIUtilities.h
    platform/win/HWndDC.h
    platform/win/PopupMenuWin.h
    platform/win/ScrollbarThemeWin.h
    platform/win/SearchPopupMenuDB.h
    platform/win/SearchPopupMenuWin.h
    platform/win/SystemInfo.h
    platform/win/WCDataObject.h
    platform/win/WebCoreBundleWin.h
    platform/win/WebCoreInstanceHandle.h
    platform/win/WebCoreTextRenderer.h
    platform/win/WindowMessageBroadcaster.h
    platform/win/WindowMessageListener.h
    platform/win/WindowsKeyNames.h
    platform/win/WindowsTouch.h
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeWin.css
    ${WEBCORE_DIR}/css/themeWinQuirks.css
)

if (USE_CF)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/cf"
    )

    list(APPEND WebCore_SOURCES
        editing/SmartReplaceCF.cpp

        loader/archive/cf/LegacyWebArchive.cpp

        platform/cf/SharedBufferCF.cpp

        platform/text/cf/HyphenationCF.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        loader/archive/cf/LegacyWebArchive.h
    )

    list(APPEND WebCore_LIBRARIES ${COREFOUNDATION_LIBRARY})
    list(APPEND WebCoreTestSupport_LIBRARIES ${COREFOUNDATION_LIBRARY})
else ()
    list(APPEND WebCore_SOURCES
        platform/text/Hyphenation.cpp
    )
endif ()

if (USE_CFURLCONNECTION)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        ${WEBCORE_DIR}/platform/cf/win
    )
    list(APPEND WebCore_SOURCES
        platform/cf/win/CertificateCFWin.cpp
    )
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/cf/win/CertificateCFWin.h
    )
endif ()

if (USE_CF AND NOT WTF_PLATFORM_WIN_CAIRO)
    list(APPEND WebCore_SOURCES
        platform/cf/KeyedDecoderCF.cpp
        platform/cf/KeyedEncoderCF.cpp
    )
else ()
    list(APPEND WebCore_SOURCES
        platform/generic/KeyedDecoderGeneric.cpp
        platform/generic/KeyedEncoderGeneric.cpp
    )
endif ()

if (${WTF_PLATFORM_WIN_CAIRO})
    include(PlatformWinCairo.cmake)
else ()
    include(PlatformAppleWin.cmake)
endif ()

make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/en.lproj)
file(COPY
    "${WEBCORE_DIR}/en.lproj/Localizable.strings"
    "${WEBCORE_DIR}/en.lproj/mediaControlsLocalizedStrings.js"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/en.lproj
)
file(COPY
    "${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsApple.css"
    "${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsApple.js"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources
)

set(WebCore_OUTPUT_NAME WebCore${DEBUG_SUFFIX})
