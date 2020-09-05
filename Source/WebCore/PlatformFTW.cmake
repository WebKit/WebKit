include(platform/Curl.cmake)
if (NOT APPLE_BUILD)
    include(platform/ImageDecoders.cmake)
endif ()
include(platform/OpenSSL.cmake)
include(platform/TextureMapper.cmake)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${DirectX_INCLUDE_DIRS}"
    "${WEBKIT_LIBRARIES_DIR}/include"
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

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
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
    page/win/FrameWinDirect2D.cpp

    platform/Cursor.cpp
    platform/LocalizedStrings.cpp
    platform/StaticPasteboard.cpp

    platform/audio/PlatformMediaSessionManager.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/GraphicsContextImpl.cpp
    platform/graphics/PlatformDisplay.cpp

    platform/graphics/egl/GLContextEGL.cpp

    platform/graphics/opengl/ExtensionsGLOpenGLCommon.cpp
    platform/graphics/opengl/ExtensionsGLOpenGLES.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLES.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLPrivate.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeUtilities.cpp

    platform/graphics/win/BackingStoreBackendDirect2DImpl.cpp
    platform/graphics/win/ColorDirect2D.cpp
    platform/graphics/win/ComplexTextControllerDirectWrite.cpp
    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/Direct2DOperations.cpp
    platform/graphics/win/Direct2DUtilities.cpp
    platform/graphics/win/DirectWriteUtilities.cpp
    platform/graphics/win/FloatPointDirect2D.cpp
    platform/graphics/win/FloatRectDirect2D.cpp
    platform/graphics/win/FloatSizeDirect2D.cpp
    platform/graphics/win/FontCacheWin.cpp
    platform/graphics/win/FontCascadeDirect2D.cpp
    platform/graphics/win/FontCustomPlatformData.cpp
    platform/graphics/win/FontDescriptionWin.cpp
    platform/graphics/win/FontPlatformDataDirect2D.cpp
    platform/graphics/win/FontPlatformDataWin.cpp
    platform/graphics/win/FontWin.cpp
    platform/graphics/win/GlyphPageTreeNodeDirect2D.cpp
    platform/graphics/win/GradientDirect2D.cpp
    platform/graphics/win/GraphicsContextGLDirect2D.cpp
    platform/graphics/win/GraphicsContextDirect2D.cpp
    platform/graphics/win/GraphicsContextImplDirect2D.cpp
    platform/graphics/win/GraphicsContextWin.cpp
    platform/graphics/win/IconWin.cpp
    platform/graphics/win/ImageBufferDirect2DBackend.cpp
    platform/graphics/win/ImageDecoderDirect2D.cpp
    platform/graphics/win/ImageDirect2D.cpp
    platform/graphics/win/ImageWin.cpp
    platform/graphics/win/IntPointWin.cpp
    platform/graphics/win/IntRectWin.cpp
    platform/graphics/win/IntSizeWin.cpp
    platform/graphics/win/MediaPlayerPrivateMediaFoundation.cpp
    platform/graphics/win/NativeImageDirect2D.cpp
    platform/graphics/win/PathDirect2D.cpp
    platform/graphics/win/PatternDirect2D.cpp
    platform/graphics/win/PlatformContextDirect2D.cpp
    platform/graphics/win/SimpleFontDataWin.cpp
    platform/graphics/win/SimpleFontDataDirect2D.cpp
    platform/graphics/win/TextAnalyzerHelper.cpp
    platform/graphics/win/TransformationMatrixDirect2D.cpp
    platform/graphics/win/TransformationMatrixWin.cpp

    platform/network/win/CurlSSLHandleWin.cpp
    platform/network/win/DownloadBundleWin.cpp
    platform/network/win/NetworkStateNotifierWin.cpp

    platform/text/Hyphenation.cpp
    platform/text/win/LocaleWin.cpp

    platform/win/BString.cpp
    platform/win/BitmapInfo.cpp
    platform/win/ClipboardUtilitiesWin.cpp
    platform/win/CursorWin.cpp
    platform/win/DefWndProcWindowClass.cpp
    platform/win/DelayLoadedModulesEnumerator.cpp
    platform/win/DragDataWin.cpp
    platform/win/DragImageDirect2D.cpp
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
    platform/win/SSLKeyGeneratorWin.cpp
    platform/win/ScrollbarThemeWin.cpp
    platform/win/SearchPopupMenuDB.cpp
    platform/win/SearchPopupMenuWin.cpp
    platform/win/SharedBufferWin.cpp
    platform/win/StructuredExceptionHandlerSuppressor.cpp
    platform/win/SystemInfo.cpp
    platform/win/UserAgentWin.cpp
    platform/win/WCDataObject.cpp
    platform/win/WebCoreBundleWin.cpp
    platform/win/WebCoreInstanceHandle.cpp
    platform/win/WebCoreTextRenderer.cpp
    platform/win/WheelEventWin.cpp
    platform/win/WidgetWin.cpp
    platform/win/WindowMessageBroadcaster.cpp

    rendering/RenderThemeWin.cpp
)

if (USE_CF)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/loader/archive/cf"
        "${WEBCORE_DIR}/platform/cf"
        "${WEBCORE_DIR}/platform/cf/win"
    )

    list(APPEND WebCore_SOURCES
        editing/SmartReplaceCF.cpp

        loader/archive/cf/LegacyWebArchive.cpp

        platform/cf/KeyedDecoderCF.cpp
        platform/cf/KeyedEncoderCF.cpp
        platform/cf/SharedBufferCF.cpp

        platform/cf/win/CertificateCFWin.cpp

        platform/text/cf/HyphenationCF.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        loader/archive/cf/LegacyWebArchive.h

        platform/cf/win/CertificateCFWin.h
    )

    list(APPEND WebCore_LIBRARIES ${COREFOUNDATION_LIBRARY})
    list(APPEND WebCoreTestSupport_LIBRARIES ${COREFOUNDATION_LIBRARY})
else ()
    list(APPEND WebCore_SOURCES
        platform/text/Hyphenation.cpp
    )
endif ()

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/win/AccessibilityObjectWrapperWin.h

    page/win/FrameWin.h

    platform/graphics/win/BackingStoreBackendDirect2D.h
    platform/graphics/win/BackingStoreBackendDirect2DImpl.h
    platform/graphics/win/DIBPixelData.h
    platform/graphics/win/Direct2DOperations.h
    platform/graphics/win/Direct2DUtilities.h
    platform/graphics/win/DirectWriteUtilities.h
    platform/graphics/win/FullScreenController.h
    platform/graphics/win/FullScreenControllerClient.h
    platform/graphics/win/GraphicsContextImplDirect2D.h
    platform/graphics/win/ImageDecoderDirect2D.h
    platform/graphics/win/LocalWindowsContext.h
    platform/graphics/win/MediaPlayerPrivateFullscreenWindow.h
    platform/graphics/win/PlatformContextDirect2D.h
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
    platform/win/WindowsTouch.h
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeWin.css
    ${WEBCORE_DIR}/css/themeWinQuirks.css
)

list(APPEND WebCore_LIBRARIES
    ${DirectX_LIBRARIES}
    comctl32
    crypt32
    iphlpapi
    rpcrt4
    shlwapi
    usp10
    version
    winmm
    ws2_32
)

list(APPEND WebCoreTestSupport_LIBRARIES
    shlwapi
)

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

if (EXISTS ${WEBKIT_LIBRARIES_DIR}/etc/ssl/cert.pem)
    make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/certificates)
    file(COPY
        ${WEBKIT_LIBRARIES_DIR}/etc/ssl/cert.pem
        DESTINATION
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/certificates
    )
    file(RENAME
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/certificates/cert.pem
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/certificates/cacert.pem
    )
endif ()

set(WebCore_OUTPUT_NAME WebCore${DEBUG_SUFFIX})
