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
    accessibility/playstation/AXObjectCachePlayStation.cpp
    accessibility/playstation/AccessibilityObjectPlayStation.cpp

    editing/libwpe/EditorLibWPE.cpp

    page/playstation/ResourceUsageOverlayPlayStation.cpp
    page/playstation/ResourceUsageThreadPlayStation.cpp

    page/scrolling/nicosia/ScrollingCoordinatorNicosia.cpp
    page/scrolling/nicosia/ScrollingStateNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeFixedNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeFrameScrollingNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeOverflowScrollProxyNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeOverflowScrollingNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreePositionedNodeNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeScrollingNodeDelegateNicosia.cpp
    page/scrolling/nicosia/ScrollingTreeStickyNodeNicosia.cpp

    platform/ScrollAnimationKinetic.cpp
    platform/ScrollAnimationSmooth.cpp

    platform/generic/KeyedDecoderGeneric.cpp
    platform/generic/KeyedEncoderGeneric.cpp

    platform/graphics/PlatformDisplay.cpp

    platform/graphics/egl/GLContext.cpp
    platform/graphics/egl/GLContextLibWPE.cpp

    platform/graphics/libwpe/PlatformDisplayLibWPE.cpp

    platform/graphics/playstation/SystemFontDatabasePlayStation.cpp

    platform/libwpe/PasteboardLibWPE.cpp
    platform/libwpe/PlatformKeyboardEventLibWPE.cpp
    platform/libwpe/PlatformPasteboardLibWPE.cpp

    platform/network/playstation/CurlSSLHandlePlayStation.cpp
    platform/network/playstation/NetworkStateNotifierPlayStation.cpp

    platform/playstation/MIMETypeRegistryPlayStation.cpp
    platform/playstation/PlatformScreenPlayStation.cpp
    platform/playstation/ScrollbarThemePlayStation.cpp
    platform/playstation/ThemePlayStation.cpp
    platform/playstation/UserAgentPlayStation.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp

    platform/unix/LoggingUnix.cpp
    platform/unix/SharedMemoryUnix.cpp

    rendering/playstation/RenderThemePlayStation.cpp
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/mediaControls.css
)

list(APPEND WebCore_LIBRARIES
    WPE::libwpe
    WebKitRequirements::WebKitResources
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/libwpe/PlatformDisplayLibWPE.h
)

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/gamepad/libwpe"
    )

    list(APPEND WebCore_SOURCES
        platform/gamepad/libwpe/GamepadLibWPE.cpp
        platform/gamepad/libwpe/GamepadProviderLibWPE.cpp
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/gamepad/libwpe/GamepadProviderLibWPE.h
    )
endif ()

# Find the extras needed to copy for EGL besides the libraries
set(EGL_EXTRAS)
foreach (EGL_EXTRA_NAME ${EGL_EXTRA_NAMES})
    find_file(${EGL_EXTRA_NAME}_FOUND ${EGL_EXTRA_NAME} PATH_SUFFIXES bin)
    if (${EGL_EXTRA_NAME}_FOUND)
        set(_src ${${EGL_EXTRA_NAME}_FOUND})
        get_filename_component(_filename ${_src} NAME)
        set(_dst "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_filename}")

        add_custom_command(OUTPUT ${_dst}
            COMMAND ${CMAKE_COMMAND} -E copy ${_src} ${_dst}
            MAIN_DEPENDENCY ${_src}
            VERBATIM
        )

        list(APPEND EGL_EXTRAS ${_dst})
    endif ()
endforeach ()

if (EGL_EXTRAS)
    add_custom_target(EGLExtras_Copy ALL DEPENDS ${EGL_EXTRAS})
    set_target_properties(EGLExtras_Copy PROPERTIES FOLDER "PlayStation")
    list(APPEND WebCore_INTERFACE_DEPENDENCIES EGLExtras_Copy)
endif ()

set(WebCore_MODULES
    Brotli
    CURL
    Cairo
    EGL
    Fontconfig
    Freetype
    HarfBuzz
    ICU
    JPEG
    LibPSL
    LibXml2
    OpenSSL
    PNG
    SQLite
    WebKitRequirements
    WebP
)

if (USE_WPE_BACKEND_PLAYSTATION)
    list(APPEND WebCore_MODULES WPE)
endif ()

find_library(SHOWMAP_LIB showmap)
list(APPEND WebCore_LIBRARIES ${SHOWMAP_LIB})

find_path(SHOWMAP_INCLUDE_DIR NAMES showmap.h)
list(APPEND WebCore_INCLUDE_DIRECTORIES ${SHOWMAP_INCLUDE_DIR})

PLAYSTATION_COPY_MODULES(WebCore TARGETS ${WebCore_MODULES})
