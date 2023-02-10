add_definitions(/bigobj -D__STDC_CONSTANT_MACROS)

include(platform/Adwaita.cmake)
include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/ImageDecoders.cmake)
include(platform/OpenSSL.cmake)
include(platform/TextureMapper.cmake)

if (USE_DAWN)
    include(platform/Dawn.cmake)
endif ()

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/win"
    "${WEBCORE_DIR}/page/win"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/wc"
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
    page/win/FrameCairoWin.cpp
    page/win/FrameWin.cpp
    page/win/ResourceUsageOverlayWin.cpp
    page/win/ResourceUsageThreadWin.cpp

    platform/Cursor.cpp
    platform/LocalizedStrings.cpp
    platform/StaticPasteboard.cpp

    platform/audio/PlatformMediaSessionManager.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/PlatformDisplay.cpp

    platform/graphics/egl/GLContextEGL.cpp

    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeUtilities.cpp

    platform/graphics/win/ComplexTextControllerUniscribe.cpp
    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/DisplayRefreshMonitorWin.cpp
    platform/graphics/win/DrawGlyphsRecorderWin.cpp
    platform/graphics/win/FloatRectWin.cpp
    platform/graphics/win/FontCacheWin.cpp
    platform/graphics/win/FontCustomPlatformDataWin.cpp
    platform/graphics/win/FontDescriptionWin.cpp
    platform/graphics/win/FontPlatformDataCairoWin.cpp
    platform/graphics/win/FontPlatformDataWin.cpp
    platform/graphics/win/FontWin.cpp
    platform/graphics/win/FullScreenController.cpp
    platform/graphics/win/FullScreenWindow.cpp
    platform/graphics/win/GlyphPageTreeNodeCairoWin.cpp
    platform/graphics/win/GraphicsContextCairoWin.cpp
    platform/graphics/win/GraphicsContextWin.cpp
    platform/graphics/win/IconWin.cpp
    platform/graphics/win/ImageCairoWin.cpp
    platform/graphics/win/ImageWin.cpp
    platform/graphics/win/IntPointWin.cpp
    platform/graphics/win/IntRectWin.cpp
    platform/graphics/win/IntSizeWin.cpp
    platform/graphics/win/MediaPlayerPrivateMediaFoundation.cpp
    platform/graphics/win/PlatformDisplayWin.cpp
    platform/graphics/win/SimpleFontDataCairoWin.cpp
    platform/graphics/win/SimpleFontDataWin.cpp
    platform/graphics/win/SystemFontDatabaseWin.cpp
    platform/graphics/win/TransformationMatrixWin.cpp

    platform/network/win/CurlSSLHandleWin.cpp
    platform/network/win/DownloadBundleWin.cpp
    platform/network/win/NetworkStateNotifierWin.cpp

    platform/text/win/LocaleWin.cpp

    platform/win/BString.cpp
    platform/win/BitmapInfo.cpp
    platform/win/ClipboardUtilitiesWin.cpp
    platform/win/CursorWin.cpp
    platform/win/DefWndProcWindowClass.cpp
    platform/win/DelayLoadedModulesEnumerator.cpp
    platform/win/DragDataWin.cpp
    platform/win/DragImageCairoWin.cpp
    platform/win/DragImageWin.cpp
    platform/win/GDIObjectCounter.cpp
    platform/win/GDIUtilities.cpp
    platform/win/ImportedFunctionsEnumerator.cpp
    platform/win/ImportedModulesEnumerator.cpp
    platform/win/KeyEventWin.cpp
    platform/win/LocalizedStringsWin.cpp
    platform/win/LoggingWin.cpp
    platform/win/MIMETypeRegistryWin.cpp
    platform/win/MainThreadSharedTimerWin.cpp
    platform/win/PEImage.cpp
    platform/win/PasteboardWin.cpp
    platform/win/PlatformMouseEventWin.cpp
    platform/win/PlatformScreenWin.cpp
    platform/win/PopupMenuWin.cpp
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
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/win/AccessibilityObjectWrapperWin.h

    page/win/FrameWin.h

    platform/graphics/opentype/FontMemoryResource.h

    platform/graphics/win/DIBPixelData.h
    platform/graphics/win/FullScreenController.h
    platform/graphics/win/FullScreenControllerClient.h
    platform/graphics/win/FullScreenWindow.h
    platform/graphics/win/GraphicsContextWin.h
    platform/graphics/win/LocalWindowsContext.h
    platform/graphics/win/SharedGDIObject.h

    platform/win/BString.h
    platform/win/BitmapInfo.h
    platform/win/COMPtr.h
    platform/win/DefWndProcWindowClass.h
    platform/win/GDIObjectCounter.h
    platform/win/GDIUtilities.h
    platform/win/HWndDC.h
    platform/win/PopupMenuWin.h
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

list(APPEND WebCore_LIBRARIES
    crypt32
    iphlpapi
    usp10
)



file(COPY ${ModernMediaControlsImageFiles}
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/media-controls
)

if (USE_CF)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/loader/archive/cf"
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

    list(APPEND WebCore_LIBRARIES Apple::CoreFoundation)
    list(APPEND WebCoreTestSupport_LIBRARIES Apple::CoreFoundation)
else ()
    list(APPEND WebCore_SOURCES
        platform/text/Hyphenation.cpp
    )
endif ()

if (ENABLE_VIDEO AND USE_MEDIA_FOUNDATION)
    # Define a INTERFACE library for MediaFoundation and link it
    # explicitly with direct WebCore consumers because /DELAYLOAD causes
    # linker warnings for modules not using MediaFoundation.
    #  LINK : warning LNK4199: /DELAYLOAD:mf.dll ignored; no imports found from mf.dll
    add_library(MediaFoundation INTERFACE)
    target_link_libraries(MediaFoundation INTERFACE
        d3d9
        delayimp
        dxva2
        evr
        mf
        mfplat
        mfuuid
        strmiids
    )
    target_link_options(MediaFoundation INTERFACE
        /DELAYLOAD:d3d9.dll
        /DELAYLOAD:dxva2.dll
        /DELAYLOAD:evr.dll
        /DELAYLOAD:mf.dll
        /DELAYLOAD:mfplat.dll
    )

    list(APPEND WebCore_PRIVATE_LIBRARIES MediaFoundation)
endif ()

if (USE_WOFF2)
    # The WOFF2 libraries don't compile as DLLs on Windows, so add in
    # the additional libraries WOFF2::dec requires
    list(APPEND WebCore_LIBRARIES
        WOFF2::common
        brotlidec
    )
endif ()

list(APPEND WebCoreTestSupport_LIBRARIES
    Cairo::Cairo
    shlwapi
)

file(COPY
    "${WEBCORE_DIR}/en.lproj/Localizable.strings"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/en.lproj
)
