include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Soup.cmake)
include(platform/TextureMapper.cmake)

set(WebCore_OUTPUT_NAME WebCoreGTK)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTK.txt"

    "platform/SourcesGLib.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/atk"
    "${WEBCORE_DIR}/editing/atk"
    "${WEBCORE_DIR}/page/gtk"
    "${WEBCORE_DIR}/platform/generic"
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
    "${WEBCORE_DIR}/platform/network/gtk"
    "${WEBCORE_DIR}/platform/text/gtk"
)

if (USE_WPE_RENDERER)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/libwpe"
    )
endif ()

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/x11/PlatformDisplayX11.h
    platform/graphics/x11/XErrorTrapper.h
    platform/graphics/x11/XUniquePtr.h
    platform/graphics/x11/XUniqueResource.h

    platform/gtk/CompositionResults.h
    platform/gtk/GRefPtrGtk.h
    platform/gtk/GUniquePtrGtk.h
    platform/gtk/GtkUtilities.h
    platform/gtk/PasteboardHelper.h
    platform/gtk/SelectionData.h

    platform/text/enchant/TextCheckerEnchant.h
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/mediaControlsGtk.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/en.lproj/mediaControlsLocalizedStrings.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsGtk.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/gtk/RenderThemeGtk.cpp)

list(APPEND WebCore_LIBRARIES
    ${ATK_LIBRARIES}
    ${ENCHANT_LIBRARIES}
    ${GDK_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GTK_LIBRARIES}
    ${LIBSECCOMP_LIBRARIES}
    ${LIBSECRET_LIBRARIES}
    ${LIBTASN1_LIBRARIES}
    ${HYPHEN_LIBRARIES}
    ${UPOWERGLIB_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_Xcomposite_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xt_LIB}
    ${ZLIB_LIBRARIES}
)

if (USE_WPE_RENDERER)
    list(APPEND WebCore_LIBRARIES
        ${WPE_LIBRARIES}
    )
endif ()

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${ENCHANT_INCLUDE_DIRS}
    ${GDK_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${LIBSECCOMP_INCLUDE_DIRS}
    ${LIBSECRET_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${UPOWERGLIB_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

if (USE_WPE_RENDERER)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${WPE_INCLUDE_DIRS}
    )
endif ()

if (USE_OPENGL_ES)
    list(APPEND WebCore_SOURCES
        platform/graphics/opengl/Extensions3DOpenGLES.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
    )
endif ()

if (USE_OPENGL)
    list(APPEND WebCore_SOURCES
        platform/graphics/OpenGLShims.cpp

        platform/graphics/opengl/Extensions3DOpenGL.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
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

include_directories(SYSTEM
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
)

list(APPEND WebCoreTestSupport_LIBRARIES PRIVATE ${GTK_LIBRARIES})
list(APPEND WebCoreTestSupport_SYSTEM_INCLUDE_DIRECTORIES ${GTK_INCLUDE_DIRS})

add_definitions(-DBUILDING_WEBKIT)

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/ScrollAnimationSmooth.cpp
    )
endif ()
