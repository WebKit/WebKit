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

    page/playstation/ResourceUsageOverlayPlayStation.cpp
    page/playstation/ResourceUsageThreadPlayStation.cpp

    page/scrolling/nicosia/ScrollingCoordinatorNicosia.cpp
    page/scrolling/nicosia/ScrollingStateNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeFixedNode.cpp
    page/scrolling/nicosia/ScrollingTreeFrameScrollingNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeOverflowScrollProxyNode.cpp
    page/scrolling/nicosia/ScrollingTreeOverflowScrollingNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreePositionedNode.cpp
    page/scrolling/nicosia/ScrollingTreeScrollingNodeDelegateNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeStickyNodeNicosia.cpp

    platform/ScrollAnimationKinetic.cpp
    platform/ScrollAnimationSmooth.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/PlatformDisplay.cpp

    platform/graphics/egl/GLContextEGL.cpp
    platform/graphics/egl/GLContextEGLLibWPE.cpp

    platform/graphics/libwpe/PlatformDisplayLibWPE.cpp

    platform/libwpe/PasteboardLibWPE.cpp
    platform/libwpe/PlatformKeyboardEventLibWPE.cpp
    platform/libwpe/PlatformPasteboardLibWPE.cpp

    platform/network/playstation/CurlSSLHandlePlayStation.cpp
    platform/network/playstation/NetworkStateNotifierPlayStation.cpp

    platform/playstation/MIMETypeRegistryPlayStation.cpp
    platform/playstation/PlatformScreenPlayStation.cpp
    platform/playstation/ScrollbarThemePlayStation.cpp
    platform/playstation/UserAgentPlayStation.cpp
    platform/playstation/WidgetPlayStation.cpp

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

# Find the extras needed to copy for EGL besides the libraries
set(EGL_EXTRAS)
foreach (EGL_EXTRA_NAME ${EGL_EXTRA_NAMES})
    find_file(${EGL_EXTRA_NAME}_FOUND ${EGL_EXTRA_NAME} PATH_SUFFIXES bin)
    if (${EGL_EXTRA_NAME}_FOUND)
        list(APPEND EGL_EXTRAS ${${EGL_EXTRA_NAME}_FOUND})
    endif ()
endforeach ()

PLAYSTATION_COPY_REQUIREMENTS(WebCore_CopySharedLibs
    FILES
        ${CURL_LIBRARIES}
        ${Cairo_LIBRARIES}
        ${EGL_LIBRARIES}
        ${EGL_EXTRAS}
        ${FREETYPE_LIBRARIES}
        ${Fontconfig_LIBRARIES}
        ${HarfBuzz_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${PNG_LIBRARIES}
        ${WebKitRequirements_LIBRARY}
        ${WebP_LIBRARIES}
)
