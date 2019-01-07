include(platform/Cairo.cmake)
include(platform/Curl.cmake)
include(platform/FreeType.cmake)
include(platform/ImageDecoders.cmake)
include(platform/TextureMapper.cmake)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    ${THIRDPARTY_DIR}/ANGLE/
    ${THIRDPARTY_DIR}/ANGLE/include/KHR
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
    page/scrolling/nicosia/ScrollingTreeStickyNode.cpp

    page/scrolling/generic/ScrollingThreadGeneric.cpp

    platform/ScrollAnimationKinetic.cpp
    platform/ScrollAnimationSmooth.cpp
    platform/UserAgentQuirks.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp
    platform/generic/ScrollAnimatorGeneric.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/GraphicsContext3DPrivate.cpp
    platform/graphics/PlatformDisplay.cpp

    platform/graphics/egl/GLContextEGL.cpp
    platform/graphics/egl/GLContextEGLLibWPE.cpp

    platform/graphics/libwpe/PlatformDisplayLibWPE.cpp

    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/Extensions3DOpenGLES.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/libwpe/PasteboardLibWPE.cpp
    platform/libwpe/PlatformKeyboardEventLibWPE.cpp
    platform/libwpe/PlatformPasteboardLibWPE.cpp

    platform/network/playstation/CurlSSLHandlePlayStation.cpp
    platform/network/playstation/NetworkStateNotifierPlayStation.cpp

    platform/playstation/EventLoopPlayStation.cpp
    platform/playstation/MIMETypeRegistryPlayStation.cpp
    platform/playstation/PlatformScreenPlayStation.cpp
    platform/playstation/ScrollbarThemePlayStation.cpp
    platform/playstation/UserAgentPlayStation.cpp

    platform/posix/FileSystemPOSIX.cpp
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

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)

list(APPEND WebCore_LIBRARIES
    ${ICU_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${WPE_LIBRARIES}
)
