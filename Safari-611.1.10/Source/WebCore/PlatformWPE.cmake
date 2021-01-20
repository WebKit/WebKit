include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Soup.cmake)
include(platform/TextureMapper.cmake)

if (USE_EXTERNAL_HOLEPUNCH)
    include(platform/HolePunch.cmake)
endif ()

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesWPE.txt"

    "platform/SourcesGLib.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/atk"
    "${WEBCORE_DIR}/platform/adwaita"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/epoxy"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/libwpe"
    "${WEBCORE_DIR}/platform/graphics/wayland"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediastream/gstreamer"
    "${WEBCORE_DIR}/platform/network/glib"
    "${WEBCORE_DIR}/platform/text/icu"
    "${WEBCORE_DIR}/platform/wpe"
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/wayland/PlatformDisplayWayland.h
    platform/graphics/wayland/WlUniquePtr.h
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeAdwaita.css
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsAdwaita.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsAdwaita.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/wpe/RenderThemeWPE.cpp)

list(APPEND WebCore_LIBRARIES
    WPE::libwpe
    ${ATK_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBTASN1_LIBRARIES}
    ${UPOWERGLIB_LIBRARIES}
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${UPOWERGLIB_INCLUDE_DIRS}
)

if (USE_WPE_VIDEO_PLANE_DISPLAY_DMABUF OR USE_WPEBACKEND_FDO_AUDIO_EXTENSION)
    list(APPEND WebCore_LIBRARIES ${WPEBACKEND_FDO_LIBRARIES})
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES ${WPE_INCLUDE_DIRS})
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES ${WPEBACKEND_FDO_INCLUDE_DIRS})
endif ()

if (USE_OPENXR)
    list(APPEND WebCore_LIBRARIES ${OPENXR_LIBRARIES})
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES ${OPENXR_INCLUDE_DIRS})
endif ()
