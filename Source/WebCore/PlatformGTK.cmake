include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/TextureMapper.cmake)

set(WebCore_OUTPUT_NAME WebCoreGTK)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTK.txt"

    "platform/SourcesGLib.txt"
    "platform/SourcesSoup.txt"
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${THIRDPARTY_DIR}/ANGLE/"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/accessibility/atk"
    "${WEBCORE_DIR}/editing/atk"
    "${WEBCORE_DIR}/page/gtk"
    "${WEBCORE_DIR}/platform/geoclue"
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
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/gtk"
)

list(APPEND WebCorePlatformGTK_SOURCES
    editing/gtk/EditorGtk.cpp

    page/gtk/DragControllerGtk.cpp

    platform/glib/EventHandlerGLib.cpp

    platform/graphics/PlatformDisplay.cpp

    platform/graphics/gtk/ColorGtk.cpp
    platform/graphics/gtk/DisplayRefreshMonitorGtk.cpp
    platform/graphics/gtk/GdkCairoUtilities.cpp
    platform/graphics/gtk/IconGtk.cpp
    platform/graphics/gtk/ImageBufferGtk.cpp
    platform/graphics/gtk/ImageGtk.cpp

    platform/gtk/CursorGtk.cpp
    platform/gtk/DragImageGtk.cpp
    platform/gtk/GRefPtrGtk.cpp
    platform/gtk/GtkUtilities.cpp
    platform/gtk/GtkVersioning.c
    platform/gtk/PasteboardHelper.cpp
    platform/gtk/PlatformKeyboardEventGtk.cpp
    platform/gtk/PlatformMouseEventGtk.cpp
    platform/gtk/PlatformPasteboardGtk.cpp
    platform/gtk/PlatformScreenGtk.cpp
    platform/gtk/PlatformWheelEventGtk.cpp
    platform/gtk/RenderThemeGadget.cpp
    platform/gtk/RenderThemeWidget.cpp
    platform/gtk/ScrollbarThemeGtk.cpp
    platform/gtk/WidgetGtk.cpp

    rendering/RenderThemeGtk.cpp
)

if (ENABLE_GEOLOCATION)
    list(APPEND WebCore_SOURCES
        ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c
    )
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable dbus_interface geoclue-2.0 OUTPUT_VARIABLE GEOCLUE_DBUS_INTERFACE)
    add_custom_command(
         OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.h
         COMMAND gdbus-codegen --interface-prefix org.freedesktop.GeoClue2. --c-namespace Geoclue --generate-c-code ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface ${GEOCLUE_DBUS_INTERFACE}
    )
    set_source_files_properties(${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c PROPERTIES COMPILE_FLAGS -Wno-unused-parameter)
endif ()

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/mediaControlsGtk.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/English.lproj/mediaControlsLocalizedStrings.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsGtk.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/gtk/RenderThemeGtk.cpp)

list(APPEND WebCore_LIBRARIES
    ${ATK_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${ENCHANT_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBSECCOMP_LIBRARIES}
    ${LIBSECRET_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
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

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${ENCHANT_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSECCOMP_INCLUDE_DIRS}
    ${LIBSECRET_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${UPOWERGLIB_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

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

if (ENABLE_PLUGIN_PROCESS_GTK2)
    # WebKitPluginProcess2 needs a version of WebCore compiled against GTK+2, so we've isolated all the GTK+
    # dependent files into a separate library which can be used to construct a GTK+2 WebCore
    # for the plugin process.
    add_library(WebCorePlatformGTK2 ${WebCore_LIBRARY_TYPE} ${WebCorePlatformGTK_SOURCES})
    add_dependencies(WebCorePlatformGTK2 WebCore)
    set_property(TARGET WebCorePlatformGTK2
        APPEND
        PROPERTY COMPILE_DEFINITIONS GTK_API_VERSION_2=1
    )
    target_include_directories(WebCorePlatformGTK2 PRIVATE
        ${WebCore_INCLUDE_DIRECTORIES}
    )
    target_include_directories(WebCorePlatformGTK2 SYSTEM PRIVATE
        ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
        ${GTK2_INCLUDE_DIRS}
        ${GDK2_INCLUDE_DIRS}
    )
    target_link_libraries(WebCorePlatformGTK2
         ${WebCore_LIBRARIES}
         ${GTK2_LIBRARIES}
         ${GDK2_LIBRARIES}
    )
endif ()

if (ENABLE_WAYLAND_TARGET)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${WAYLAND_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${WAYLAND_LIBRARIES}
    )
endif ()

add_library(WebCorePlatformGTK ${WebCore_LIBRARY_TYPE} ${WebCorePlatformGTK_SOURCES})
add_dependencies(WebCorePlatformGTK WebCore)
target_include_directories(WebCorePlatformGTK PRIVATE
    ${WebCore_INCLUDE_DIRECTORIES}
)
target_include_directories(WebCorePlatformGTK SYSTEM PRIVATE
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
    ${GTK_INCLUDE_DIRS}
    ${GDK_INCLUDE_DIRS}
)
target_link_libraries(WebCorePlatformGTK
    ${WebCore_LIBRARIES}
    ${GTK_LIBRARIES}
    ${GDK_LIBRARIES}
)

include_directories(
    ${WebCore_INCLUDE_DIRECTORIES}
    "${WEBCORE_DIR}/bindings/gobject/"
)

include_directories(SYSTEM
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
)

add_definitions(-DBUILDING_WEBKIT)

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/ScrollAnimationSmooth.cpp
    )
endif ()
