include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Linux.cmake)
include(platform/TextureMapper.cmake)

# Allow building ANGLE on platforms that don't provide X11 headers.
list(APPEND ANGLE_PLATFORM_DEFINITIONS "USE_WPE")

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector"
    ${JAVASCRIPTCORE_DIR}
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/domjit"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/debugger"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${JAVASCRIPTCORE_DIR}/yarr"
    "${THIRDPARTY_DIR}/ANGLE/"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/epoxy"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/wpe"
    "${WEBCORE_DIR}/platform/graphics/wayland"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/icu"
    ${WTF_DIR}
)

list(APPEND WebCore_SOURCES
    accessibility/wpe/AXObjectCacheWPE.cpp
    accessibility/wpe/AccessibilityObjectWPE.cpp

    loader/soup/CachedRawResourceSoup.cpp
    loader/soup/SubresourceLoaderSoup.cpp

    page/linux/ResourceUsageOverlayLinux.cpp
    page/linux/ResourceUsageThreadLinux.cpp

    page/scrolling/ScrollingStateStickyNode.cpp
    page/scrolling/ScrollingThread.cpp
    page/scrolling/ScrollingTreeNode.cpp
    page/scrolling/ScrollingTreeScrollingNode.cpp

    page/scrolling/coordinatedgraphics/ScrollingCoordinatorCoordinatedGraphics.cpp
    page/scrolling/coordinatedgraphics/ScrollingStateNodeCoordinatedGraphics.cpp

    platform/Cursor.cpp
    platform/PlatformStrategies.cpp
    platform/Theme.cpp
    platform/UserAgentQuirks.cpp

    platform/audio/glib/AudioBusGLib.cpp

    platform/glib/EventLoopGlib.cpp
    platform/glib/FileSystemGlib.cpp
    platform/glib/KeyedDecoderGlib.cpp
    platform/glib/KeyedEncoderGlib.cpp
    platform/glib/MainThreadSharedTimerGLib.cpp
    platform/glib/SSLKeyGeneratorGLib.cpp
    platform/glib/SharedBufferGlib.cpp
    platform/glib/UserAgentGLib.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/GraphicsContext3DPrivate.cpp
    platform/graphics/ImageSource.cpp
    platform/graphics/PlatformDisplay.cpp
    platform/graphics/WOFFFileFormat.cpp

    platform/graphics/egl/GLContextEGL.cpp

    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/Extensions3DOpenGLES.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeVerticalData.cpp

    platform/graphics/wpe/PlatformDisplayWPE.cpp

    platform/network/glib/NetworkStateNotifierGLib.cpp

    platform/network/soup/AuthenticationChallengeSoup.cpp
    platform/network/soup/CertificateInfo.cpp
    platform/network/soup/CookieJarSoup.cpp
    platform/network/soup/CookieStorageSoup.cpp
    platform/network/soup/CredentialStorageSoup.cpp
    platform/network/soup/DNSSoup.cpp
    platform/network/soup/GRefPtrSoup.cpp
    platform/network/soup/NetworkStorageSessionSoup.cpp
    platform/network/soup/ProxyServerSoup.cpp
    platform/network/soup/ResourceErrorSoup.cpp
    platform/network/soup/ResourceHandleSoup.cpp
    platform/network/soup/ResourceRequestSoup.cpp
    platform/network/soup/ResourceResponseSoup.cpp
    platform/network/soup/SocketStreamHandleImplSoup.cpp
    platform/network/soup/SoupNetworkSession.cpp
    platform/network/soup/SynchronousLoaderClientSoup.cpp
    platform/network/soup/WebKitSoupRequestGeneric.cpp

    platform/soup/PublicSuffixSoup.cpp
    platform/soup/SharedBufferSoup.cpp
    platform/soup/URLSoup.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp
    platform/text/TextCodecICU.cpp
    platform/text/TextEncodingDetectorICU.cpp

    platform/unix/LoggingUnix.cpp

    platform/xdg/MIMETypeRegistryXdg.cpp
)

list(APPEND WebCorePlatformWPE_SOURCES
    editing/wpe/EditorWPE.cpp

    platform/glib/EventHandlerGLib.cpp

    platform/graphics/egl/GLContextEGLWPE.cpp

    platform/graphics/wpe/IconWPE.cpp
    platform/graphics/wpe/ImageWPE.cpp

    platform/wpe/CursorWPE.cpp
    platform/wpe/LocalizedStringsWPE.cpp
    platform/wpe/PasteboardWPE.cpp
    platform/wpe/PlatformKeyboardEventWPE.cpp
    platform/wpe/PlatformPasteboardWPE.cpp
    platform/wpe/PlatformScreenWPE.cpp
    platform/wpe/RenderThemeWPE.cpp
    platform/wpe/ScrollbarThemeWPE.cpp
    platform/wpe/ThemeWPE.cpp
    platform/wpe/WidgetWPE.cpp
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/English.lproj/mediaControlsLocalizedStrings.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/wpe/RenderThemeWPE.cpp)

list(APPEND WebCore_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${ICU_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBTASN1_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${WPE_LIBRARIES}
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
    ${WPE_INCLUDE_DIRS}
)

add_library(WebCorePlatformWPE ${WebCore_LIBRARY_TYPE} ${WebCorePlatformWPE_SOURCES})
add_dependencies(WebCorePlatformWPE WebCore)
target_include_directories(WebCorePlatformWPE PRIVATE
    ${WebCore_INCLUDE_DIRECTORIES}
)
target_include_directories(WebCorePlatformWPE SYSTEM PRIVATE
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
)
target_link_libraries(WebCorePlatformWPE
    ${WebCore_LIBRARIES}
)
