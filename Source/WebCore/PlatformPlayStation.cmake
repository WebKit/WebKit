include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/FreeType.cmake)
include(platform/ImageDecoders.cmake)
include(platform/OpenSSL.cmake)
include(platform/TextureMapper.cmake)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/generic
    ${WEBCORE_DIR}/platform/graphics/egl
    ${WEBCORE_DIR}/platform/graphics/opengl
    ${WEBCORE_DIR}/platform/graphics/libwpe
    ${WEBCORE_DIR}/platform/mediacapabilities
)

list(APPEND WebCore_SOURCES
    editing/libwpe/EditorLibWPE.cpp

    page/scrolling/nicosia/ScrollingCoordinatorNicosia.cpp
    page/scrolling/nicosia/ScrollingStateNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeFixedNode.cpp
    page/scrolling/nicosia/ScrollingTreeFrameScrollingNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeOverflowScrollProxyNode.cpp
    page/scrolling/nicosia/ScrollingTreeOverflowScrollingNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreePositionedNode.cpp
    page/scrolling/nicosia/ScrollingTreeStickyNode.cpp

    platform/ScrollAnimationKinetic.cpp
    platform/ScrollAnimationSmooth.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp
    platform/generic/ScrollAnimatorGeneric.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/PlatformDisplay.cpp

    platform/graphics/egl/GLContextEGL.cpp
    platform/graphics/egl/GLContextEGLLibWPE.cpp

    platform/graphics/libwpe/PlatformDisplayLibWPE.cpp

    platform/graphics/opengl/ExtensionsGLOpenGLCommon.cpp
    platform/graphics/opengl/ExtensionsGLOpenGLES.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLES.cpp
    platform/graphics/opengl/GraphicsContextGLOpenGLPrivate.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/libwpe/PasteboardLibWPE.cpp
    platform/libwpe/PlatformKeyboardEventLibWPE.cpp
    platform/libwpe/PlatformPasteboardLibWPE.cpp

    platform/network/playstation/CurlSSLHandlePlayStation.cpp
    platform/network/playstation/NetworkStateNotifierPlayStation.cpp

    platform/playstation/MIMETypeRegistryPlayStation.cpp
    platform/playstation/PlatformScreenPlayStation.cpp
    platform/playstation/ScrollbarThemePlayStation.cpp
    platform/playstation/UserAgentPlayStation.cpp

    platform/posix/SharedBufferPOSIX.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp

    platform/unix/LoggingUnix.cpp

    rendering/RenderThemePlayStation.cpp
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/en.lproj/mediaControlsLocalizedStrings.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.js
)

list(APPEND WebCore_LIBRARIES
    WPE::libwpe
)

PLAYSTATION_COPY_SHARED_LIBRARIES(WebCore_CopySharedLibs
    FILES
        ${CURL_LIBRARIES}
        ${Cairo_LIBRARIES}
        ${EGL_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        ${Fontconfig_LIBRARIES}
        ${HarfBuzz_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${PNG_LIBRARIES}
        ${WebKitRequirements_LIBRARY}
        ${WebP_LIBRARIES}
)
