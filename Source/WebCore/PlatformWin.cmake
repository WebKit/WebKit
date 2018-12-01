add_definitions(/bigobj -D__STDC_CONSTANT_MACROS)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
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
    "${THIRDPARTY_DIR}/ANGLE/include"
    "${THIRDPARTY_DIR}/ANGLE/include/egl"
)

list(APPEND WebCore_SOURCES
    accessibility/win/AXObjectCacheWin.cpp
    accessibility/win/AccessibilityObjectWin.cpp
    accessibility/win/AccessibilityObjectWrapperWin.cpp

    editing/SmartReplaceCF.cpp
    editing/win/EditorWin.cpp

    html/HTMLSelectElementWin.cpp

    page/win/DragControllerWin.cpp
    page/win/EventHandlerWin.cpp
    page/win/FrameWin.cpp

    platform/Cursor.cpp
    platform/LocalizedStrings.cpp
    platform/StaticPasteboard.cpp

    platform/audio/PlatformMediaSessionManager.cpp

    platform/graphics/GraphicsContext3DPrivate.cpp

    platform/graphics/egl/GLContextEGL.cpp

    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/Extensions3DOpenGLES.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeUtilities.cpp

    platform/graphics/win/ColorDirect2D.cpp
    platform/graphics/win/ComplexTextControllerDirectWrite.cpp
    platform/graphics/win/DIBPixelData.cpp
    platform/graphics/win/FloatPointDirect2D.cpp
    platform/graphics/win/FloatRectDirect2D.cpp
    platform/graphics/win/FloatSizeDirect2D.cpp
    platform/graphics/win/FontCacheWin.cpp
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
    platform/graphics/win/TransformationMatrixDirect2D.cpp
    platform/graphics/win/TransformationMatrixWin.cpp
    platform/graphics/win/UniscribeController.cpp

    platform/network/win/DownloadBundleWin.cpp
    platform/network/win/NetworkStateNotifierWin.cpp

    platform/text/LocaleNone.cpp

    platform/win/BString.cpp
    platform/win/BitmapInfo.cpp
    platform/win/ClipboardUtilitiesWin.cpp
    platform/win/CursorWin.cpp
    platform/win/DefWndProcWindowClass.cpp
    platform/win/DragDataWin.cpp
    platform/win/DragImageWin.cpp
    platform/win/EventLoopWin.cpp
    platform/win/FileSystemWin.cpp
    platform/win/GDIObjectCounter.cpp
    platform/win/GDIUtilities.cpp
    platform/win/KeyEventWin.cpp
    platform/win/LocalizedStringsWin.cpp
    platform/win/LoggingWin.cpp
    platform/win/MIMETypeRegistryWin.cpp
    platform/win/MainThreadSharedTimerWin.cpp
    platform/win/PasteboardWin.cpp
    platform/win/PathWalker.cpp
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

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeWin.css
    ${WEBCORE_DIR}/css/themeWinQuirks.css
)

set(WebCore_FORWARDING_HEADERS_DIRECTORIES
    .
    accessibility
    animation
    bindings
    bridge
    contentextensions
    crypto
    css
    dom
    editing
    fileapi
    history
    html
    inspector
    loader
    page
    platform
    plugins
    rendering
    replay
    storage
    style
    svg
    websockets
    workers
    xml

    Modules/cache
    Modules/fetch
    Modules/geolocation
    Modules/indexeddb
    Modules/mediastream
    Modules/websockets

    Modules/indexeddb/client
    Modules/indexeddb/legacy
    Modules/indexeddb/server
    Modules/indexeddb/shared
    Modules/notifications
    Modules/webdatabase

    accessibility/win

    bindings/js

    bridge/c
    bridge/jsc

    css/parser

    html/canvas
    html/forms
    html/parser
    html/shadow
    html/track

    loader/appcache
    loader/archive
    loader/cache
    loader/icon


    page/animation
    page/csp
    page/scrolling
    page/win

    platform/animation
    platform/audio
    platform/graphics
    platform/mediacapabilities
    platform/mock
    platform/network
    platform/sql
    platform/text
    platform/win

    platform/graphics/filters
    platform/graphics/opengl
    platform/graphics/opentype
    platform/graphics/texmap
    platform/graphics/transforms
    platform/graphics/win

    platform/mediastream/libwebrtc

    platform/text/transcoder

    rendering/line
    rendering/shapes
    rendering/style
    rendering/svg

    svg/animation
    svg/graphics
    svg/properties

    svg/graphics/filters

    workers/service
)

if (ENABLE_WEBKIT)
    list(APPEND WebCore_FORWARDING_HEADERS_DIRECTORIES
        Modules/applicationmanifest

        dom/messageports

        inspector/agents

        platform/mediacapabilities
        platform/mediastream

        workers/service/context
        workers/service/server
    )
endif ()

if (USE_CF)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/cf"
        "${WEBCORE_DIR}/platform/cf/win"
    )

    list(APPEND WebCore_SOURCES
        loader/archive/cf/LegacyWebArchive.cpp

        platform/cf/FileSystemCF.cpp
        platform/cf/KeyedDecoderCF.cpp
        platform/cf/KeyedEncoderCF.cpp
        platform/cf/SharedBufferCF.cpp

        platform/cf/win/CertificateCFWin.cpp

        platform/text/cf/HyphenationCF.cpp
    )

    list(APPEND WebCore_FORWARDING_HEADERS_DIRECTORIES
        history/cf

        loader/archive/cf

        platform/cf

        platform/cf/win
    )
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/makesafeseh.obj)
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/makesafeseh.obj
        DEPENDS ${WEBCORE_DIR}/platform/win/makesafeseh.asm
        COMMAND ml /safeseh /c /Fo ${DERIVED_SOURCES_WEBCORE_DIR}/makesafeseh.obj ${WEBCORE_DIR}/platform/win/makesafeseh.asm
        VERBATIM)
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
if (WTF_PLATFORM_WIN_CAIRO AND EXISTS ${WEBKIT_LIBRARIES_DIR}/etc/ssl/cert.pem)
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

WEBKIT_MAKE_FORWARDING_HEADERS(WebCore
    DIRECTORIES ${WebCore_FORWARDING_HEADERS_DIRECTORIES}
    DERIVED_SOURCE_DIRECTORIES ${DERIVED_SOURCES_WEBCORE_DIR}
    FLATTENED
)

set(WebCore_OUTPUT_NAME
    WebCore${DEBUG_SUFFIX}
)

list(APPEND WebCore_LIBRARIES WTF${DEBUG_SUFFIX})
if (TARGET libEGL)
    list(APPEND WebCore_LIBRARIES libEGL)
endif ()
list(APPEND WebCoreTestSupport_LIBRARIES WTF${DEBUG_SUFFIX})
