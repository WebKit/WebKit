add_definitions(/bigobj)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/../include/private"
    "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE/include/KHR"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/ForwardingHeaders"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/API"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/assembler"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/builtins"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/bytecode"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/bytecompiler"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/dfg"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/disassembler"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/heap"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/debugger"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/interpreter"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/jit"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/llint"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/parser"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/profiler"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/runtime"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/yarr"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/WTF"
    "${WEBCORE_DIR}/ForwardingHeaders"
    "${WEBCORE_DIR}/accessibility/win"
    "${WEBCORE_DIR}/page/win"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cf/win"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/win"
    "${WEBCORE_DIR}/platform/network/win"
    "${WEBCORE_DIR}/platform/win"
    "${WEBCORE_DIR}/plugins/win"
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

    loader/archive/cf/LegacyWebArchive.cpp

    page/win/DragControllerWin.cpp
    page/win/EventHandlerWin.cpp
    page/win/FrameWin.cpp

    platform/Cursor.cpp
    platform/LocalizedStrings.cpp
    platform/VNodeTracker.cpp

    platform/audio/PlatformMediaSessionManager.cpp

    platform/cf/CFURLExtras.cpp
    platform/cf/FileSystemCF.cpp
    platform/cf/KeyedDecoderCF.cpp
    platform/cf/KeyedEncoderCF.cpp
    platform/cf/SharedBufferCF.cpp
    platform/cf/URLCF.cpp

    platform/cf/win/CertificateCFWin.cpp

    platform/crypto/win/CryptoDigestWin.cpp

    platform/graphics/FontPlatformData.cpp
    platform/graphics/GraphicsContext3DPrivate.cpp

    platform/graphics/egl/GLContextEGL.cpp

    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/Extensions3DOpenGLES.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeUtilities.cpp

    platform/graphics/win/DIBPixelData.cpp
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
    platform/graphics/win/TransformationMatrixWin.cpp
    platform/graphics/win/UniscribeController.cpp

    platform/network/win/DownloadBundleWin.cpp
    platform/network/win/NetworkStateNotifierWin.cpp

    platform/text/LocaleNone.cpp

    platform/text/cf/HyphenationCF.cpp

    platform/text/win/TextBreakIteratorInternalICUWin.cpp

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
    platform/win/LanguageWin.cpp
    platform/win/LocalizedStringsWin.cpp
    platform/win/LoggingWin.cpp
    platform/win/MemoryPressureHandlerWin.cpp
    platform/win/MIMETypeRegistryWin.cpp
    platform/win/MainThreadSharedTimerWin.cpp
    platform/win/PasteboardWin.cpp
    platform/win/PathWalker.cpp
    platform/win/PlatformMouseEventWin.cpp
    platform/win/PlatformScreenWin.cpp
    platform/win/PopupMenuWin.cpp
    platform/win/SSLKeyGeneratorWin.cpp
    platform/win/ScrollbarThemeWin.cpp
    platform/win/SearchPopupMenuWin.cpp
    platform/win/SharedBufferWin.cpp
    platform/win/SoundWin.cpp
    platform/win/StructuredExceptionHandlerSuppressor.cpp
    platform/win/SystemInfo.cpp
    platform/win/WCDataObject.cpp
    platform/win/WebCoreBundleWin.cpp
    platform/win/WebCoreInstanceHandle.cpp
    platform/win/WebCoreTextRenderer.cpp
    platform/win/WheelEventWin.cpp
    platform/win/WidgetWin.cpp
    platform/win/WindowMessageBroadcaster.cpp
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeWin.css
    ${WEBCORE_DIR}/css/themeWinQuirks.css
)

list(APPEND WebCore_DERIVED_SOURCES
    "${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreHeaderDetection.h"
)

set(WebCore_FORWARDING_HEADERS_DIRECTORIES
    .
    accessibility
    bindings
    bridge
    contentextensions
    css
    dom
    editing
    history
    html
    inspector
    loader
    page
    platform
    plugins
    rendering
    storage
    style
    svg
    websockets
    workers
    xml

    Modules/geolocation
    Modules/indexeddb
    Modules/indexeddb/legacy
    Modules/indexeddb/shared
    Modules/notifications
    Modules/webdatabase

    accessibility/win

    bindings/generic
    bindings/js

    bridge/c
    bridge/jsc

    history/cf

    html/forms
    html/parser
    html/shadow
    html/track

    loader/appcache
    loader/archive
    loader/cache
    loader/icon

    loader/archive/cf

    page/animation
    page/csp
    page/scrolling
    page/win

    platform/animation
    platform/audio
    platform/cf
    platform/graphics
    platform/mock
    platform/network
    platform/sql
    platform/text
    platform/win

    platform/cf/win

    platform/graphics/filters
    platform/graphics/opengl
    platform/graphics/opentype
    platform/graphics/texmap
    platform/graphics/transforms
    platform/graphics/win

    platform/text/transcoder

    rendering/line
    rendering/shapes
    rendering/style
    rendering/svg

    svg/animation
    svg/graphics
    svg/properties

    svg/graphics/filters
)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/makesafeseh.obj)
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

add_custom_command(
    OUTPUT "${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreHeaderDetection.h"
    WORKING_DIRECTORY "${DERIVED_SOURCES_WEBCORE_DIR}"
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/AVFoundationSupport.py ${WEBKIT_LIBRARIES_DIR} > WebCoreHeaderDetection.h
    VERBATIM)

make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/en.lproj)
file(COPY
    "${WEBCORE_DIR}/English.lproj/Localizable.strings"
    "${WEBCORE_DIR}/English.lproj/mediaControlsLocalizedStrings.js"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/en.lproj
)
file(COPY
    "${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsApple.css"
    "${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsApple.js"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources
)
if (WTF_PLATFORM_WIN_CAIRO AND EXISTS ${WEBKIT_LIBRARIES_DIR}/cacert.pem)
    make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/certificates)
    file(COPY
        ${WEBKIT_LIBRARIES_DIR}/cacert.pem
        DESTINATION
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebKit.resources/certificates
    )
endif ()

file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebCore)

set(WebCore_DERIVED_SOURCES_PRE_BUILD_COMMAND "${CMAKE_BINARY_DIR}/DerivedSources/WebCore/preBuild.cmd")
file(WRITE "${WebCore_DERIVED_SOURCES_PRE_BUILD_COMMAND}" "@xcopy /y /s /d /f \"${WEBCORE_DIR}/ForwardingHeaders/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebCore\" >nul 2>nul\n")
foreach (_directory ${WebCore_FORWARDING_HEADERS_DIRECTORIES})
    file(APPEND "${WebCore_DERIVED_SOURCES_PRE_BUILD_COMMAND}" "@xcopy /y /d /f \"${WEBCORE_DIR}/${_directory}/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebCore\" >nul 2>nul\n")
endforeach ()

set(WebCore_POST_BUILD_COMMAND "${CMAKE_BINARY_DIR}/DerivedSources/WebCore/postBuild.cmd")
file(WRITE "${WebCore_POST_BUILD_COMMAND}" "@xcopy /y /s /d /f \"${DERIVED_SOURCES_WEBCORE_DIR}/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebCore\" >nul 2>nul\n")

set(WebCore_OUTPUT_NAME
    WebCore${DEBUG_SUFFIX}
)

list(APPEND WebCore_LIBRARIES WTF${DEBUG_SUFFIX})
list(APPEND WebCoreTestSupport_LIBRARIES WTF${DEBUG_SUFFIX})
