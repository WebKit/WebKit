include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Soup.cmake)
include(platform/TextureMapper.cmake)

set(WebCore_OUTPUT_NAME WebCoreGTK)

# FIXME: https://bugs.webkit.org/show_bug.cgi?id=181916
# Remove these lines when turning on hidden visibility
list(APPEND WebCore_PRIVATE_LIBRARIES WebKit::WTF)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebCore_PRIVATE_LIBRARIES WebKit::bmalloc)
endif ()

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTK.txt"

    "platform/SourcesGLib.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/atk"
    "${WEBCORE_DIR}/editing/atk"
    "${WEBCORE_DIR}/page/gtk"
    "${WEBCORE_DIR}/platform/adwaita"
    "${WEBCORE_DIR}/platform/generic"
    "${WEBCORE_DIR}/platform/glib"
    "${WEBCORE_DIR}/platform/gtk"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    "${WEBCORE_DIR}/platform/graphics/gtk"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/wayland"
    "${WEBCORE_DIR}/platform/graphics/x11"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediastream/gtk"
    "${WEBCORE_DIR}/platform/mediastream/gstreamer"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network/glib"
    "${WEBCORE_DIR}/platform/text/gtk"
)

if (USE_WPE_RENDERER)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/libwpe"
    )
endif ()

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/adwaita/ScrollbarThemeAdwaita.h

    platform/glib/ApplicationGLib.h

    platform/graphics/x11/PlatformDisplayX11.h
    platform/graphics/x11/XErrorTrapper.h
    platform/graphics/x11/XUniquePtr.h
    platform/graphics/x11/XUniqueResource.h

    platform/gtk/GRefPtrGtk.h
    platform/gtk/GUniquePtrGtk.h
    platform/gtk/GtkUtilities.h
    platform/gtk/GtkVersioning.h
    platform/gtk/ScrollbarThemeGtk.h
    platform/gtk/SelectionData.h

    platform/text/enchant/TextCheckerEnchant.h
)

set(CSS_VALUE_PLATFORM_DEFINES "HAVE_OS_DARK_MODE_SUPPORT=1")

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsAdwaita.css
    ${WEBCORE_DIR}/css/themeAdwaita.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsAdwaita.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/rendering/RenderThemeAdwaita.cpp)

list(APPEND WebCore_LIBRARIES
    ${ATK_LIBRARIES}
    ${ENCHANT_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBSECRET_LIBRARIES}
    ${LIBTASN1_LIBRARIES}
    ${HYPHEN_LIBRARIES}
    ${UPOWERGLIB_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_Xcomposite_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xt_LIB}
    GTK::GTK
)

if (USE_LCMS)
    list(APPEND WebCore_LIBRARIES
        LCMS2::LCMS2
    )
endif ()

if (USE_WPE_RENDERER)
    list(APPEND WebCore_LIBRARIES
        WPE::libwpe
    )
endif ()

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${ENCHANT_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSECRET_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${UPOWERGLIB_INCLUDE_DIRS}
)

if (USE_OPENGL)
    list(APPEND WebCore_SOURCES
        platform/graphics/OpenGLShims.cpp
    )
endif ()

if (ENABLE_WAYLAND_TARGET)
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/wayland/PlatformDisplayWayland.h
        platform/graphics/wayland/WlUniquePtr.h
    )
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${WAYLAND_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${WAYLAND_LIBRARIES}
    )
endif ()

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/gamepad/manette/ManetteGamepadProvider.h
    )
    list(APPEND WebCore_LIBRARIES
        Manette::Manette
    )
endif ()

if (ENABLE_BUBBLEWRAP_SANDBOX)
    list(APPEND WebCore_LIBRARIES Libseccomp::Libseccomp)
endif ()

include_directories(SYSTEM
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
)

list(APPEND WebCoreTestSupport_LIBRARIES PRIVATE GTK::GTK)

add_definitions(-DBUILDING_WEBKIT)

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/ScrollAnimationSmooth.cpp
    )
endif ()
