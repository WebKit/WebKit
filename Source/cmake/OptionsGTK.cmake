include(GNUInstallDirs)
include(VersioningUtils)

SET_PROJECT_VERSION(2 23 1)
set(WEBKITGTK_API_VERSION 4.0)

CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(WEBKIT 71 0 34)
CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(JAVASCRIPTCORE 30 0 12)

# These are shared variables, but we special case their definition so that we can use the
# CMAKE_INSTALL_* variables that are populated by the GNUInstallDirs macro.
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE PATH "Absolute path to library installation directory")
set(EXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE PATH "Absolute path to executable installation directory")
set(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/webkit2gtk-${WEBKITGTK_API_VERSION}" CACHE PATH "Absolute path to install executables executed by the library")

set(WEBKITGTK_HEADER_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/webkitgtk-${WEBKITGTK_API_VERSION}")
set(INTROSPECTION_INSTALL_GIRDIR "${CMAKE_INSTALL_FULL_DATADIR}/gir-1.0")
set(INTROSPECTION_INSTALL_TYPELIBDIR "${LIB_INSTALL_DIR}/girepository-1.0")

find_package(Cairo 1.10.2 REQUIRED)
find_package(Fontconfig 2.8.0 REQUIRED)
find_package(Freetype 2.4.2 REQUIRED)
find_package(LibGcrypt 1.6.0 REQUIRED)
find_package(GTK3 3.6.0 REQUIRED)
find_package(GDK3 3.6.0 REQUIRED)
find_package(HarfBuzz 0.9.2 REQUIRED)
find_package(ICU REQUIRED)
find_package(JPEG REQUIRED)
find_package(LibSoup 2.42.0 REQUIRED)
find_package(LibXml2 2.8.0 REQUIRED)
find_package(PNG REQUIRED)
find_package(Sqlite REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(ATK REQUIRED)
find_package(WebP REQUIRED)
find_package(ATSPI 2.5.3)
find_package(EGL)
find_package(GTKUnixPrint)
find_package(OpenGL)
find_package(OpenGLES2)

WEBKIT_OPTION_BEGIN()

include(GStreamerDefinitions)

SET_AND_EXPOSE_TO_BUILD(USE_CAIRO TRUE)
SET_AND_EXPOSE_TO_BUILD(USE_XDGMIME TRUE)
SET_AND_EXPOSE_TO_BUILD(USE_GCRYPT TRUE)

if (WTF_CPU_ARM OR WTF_CPU_MIPS)
    SET_AND_EXPOSE_TO_BUILD(USE_CAPSTONE ${DEVELOPER_MODE})
endif ()

# For old versions of HarfBuzz that do not expose an API for the OpenType MATH
# table, we enable our own code to parse that table.
if ("${PC_HARFBUZZ_VERSION}" VERSION_LESS "1.3.3")
    add_definitions(-DENABLE_OPENTYPE_MATH=1)
endif ()

# Set the default value for ENABLE_GLES2 automatically.
# We are not enabling or disabling automatically a feature here, because
# the feature is by default always on (ENABLE_OPENGL=ON).
# What we select here automatically is if we use OPENGL (ENABLE_GLES2=OFF)
# or OPENGLES2 (ENABLE_GLES2=ON) for building the feature.
set(ENABLE_GLES2_DEFAULT OFF)

if (NOT OPENGL_FOUND AND OPENGLES2_FOUND)
    set(ENABLE_GLES2_DEFAULT ON)
endif ()

# Public options specific to the GTK+ port. Do not add any options here unless
# there is a strong reason we should support changing the value of the option,
# and the option is not relevant to any other WebKit ports.
WEBKIT_OPTION_DEFINE(ENABLE_GLES2 "Whether to enable OpenGL ES 2.0." PUBLIC ${ENABLE_GLES2_DEFAULT})
WEBKIT_OPTION_DEFINE(ENABLE_GTKDOC "Whether or not to use generate gtkdoc." PUBLIC OFF)
WEBKIT_OPTION_DEFINE(ENABLE_INTROSPECTION "Whether to enable GObject introspection." PUBLIC ON)
WEBKIT_OPTION_DEFINE(ENABLE_OPENGL "Whether to use OpenGL." PUBLIC ON)
WEBKIT_OPTION_DEFINE(ENABLE_PLUGIN_PROCESS_GTK2 "Whether to build WebKitPluginProcess2 to load GTK2 based plugins." PUBLIC ON)
WEBKIT_OPTION_DEFINE(ENABLE_QUARTZ_TARGET "Whether to enable support for the Quartz windowing target." PUBLIC ${GTK3_SUPPORTS_QUARTZ})
WEBKIT_OPTION_DEFINE(ENABLE_X11_TARGET "Whether to enable support for the X11 windowing target." PUBLIC ${GTK3_SUPPORTS_X11})
WEBKIT_OPTION_DEFINE(ENABLE_WAYLAND_TARGET "Whether to enable support for the Wayland windowing target." PUBLIC ${GTK3_SUPPORTS_WAYLAND})
WEBKIT_OPTION_DEFINE(USE_LIBNOTIFY "Whether to enable the default web notification implementation." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_LIBHYPHEN "Whether to enable the default automatic hyphenation implementation." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_LIBSECRET "Whether to enable the persistent credential storage using libsecret." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_WOFF2 "Whether to enable support for WOFF2 Web Fonts." PUBLIC ON)

# Private options specific to the GTK+ port. Changing these options is
# completely unsupported. They are intended for use only by WebKit developers.
WEBKIT_OPTION_DEFINE(USE_REDIRECTED_XCOMPOSITE_WINDOW "Whether to use a Redirected XComposite Window for accelerated compositing in X11." PRIVATE ON)
WEBKIT_OPTION_DEFINE(USE_OPENVR "Whether to use OpenVR as WebVR backend." PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})

# FIXME: Can we use cairo-glesv2 to avoid this conflict?
WEBKIT_OPTION_CONFLICT(ENABLE_ACCELERATED_2D_CANVAS ENABLE_GLES2)

WEBKIT_OPTION_DEPEND(ENABLE_3D_TRANSFORMS ENABLE_OPENGL)
WEBKIT_OPTION_DEPEND(ENABLE_ACCELERATED_2D_CANVAS ENABLE_OPENGL)
WEBKIT_OPTION_DEPEND(ENABLE_ASYNC_SCROLLING ENABLE_OPENGL)
WEBKIT_OPTION_DEPEND(ENABLE_GLES2 ENABLE_OPENGL)
WEBKIT_OPTION_DEPEND(ENABLE_PLUGIN_PROCESS_GTK2 ENABLE_X11_TARGET)
WEBKIT_OPTION_DEPEND(ENABLE_WEBGL ENABLE_OPENGL)
WEBKIT_OPTION_DEPEND(USE_REDIRECTED_XCOMPOSITE_WINDOW ENABLE_OPENGL)
WEBKIT_OPTION_DEPEND(USE_REDIRECTED_XCOMPOSITE_WINDOW ENABLE_X11_TARGET)

SET_AND_EXPOSE_TO_BUILD(ENABLE_DEVELOPER_MODE ${DEVELOPER_MODE})
if (DEVELOPER_MODE)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PUBLIC ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE ON)
else ()
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PUBLIC OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE OFF)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEMORY_SAMPLER PRIVATE ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_USAGE PRIVATE ON)
else ()
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEMORY_SAMPLER PRIVATE OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_USAGE PRIVATE OFF)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT EXISTS "/.flatpak-info")
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_BUBBLEWRAP_SANDBOX PUBLIC ON)
else ()
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_BUBBLEWRAP_SANDBOX PRIVATE OFF)
endif ()

# Public options shared with other WebKit ports. Do not add any options here
# without approval from a GTK+ reviewer. There must be strong reason to support
# changing the value of the option.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ACCELERATED_2D_CANVAS PUBLIC OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DRAG_SUPPORT PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GEOLOCATION PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ICONDATABASE PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SPELLCHECK PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TOUCH_EVENTS PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_CRYPTO PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER PUBLIC ON)

# Private options shared with other WebKit ports. Add options here when
# we need a value different from the default defined in WebKitFeatures.cmake.
# Changing these options is completely unsupported.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DOWNLOAD_ATTRIBUTE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ENCRYPTED_MEDIA PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTPDIR PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_COLOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MHTML PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_STREAM PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_WORKER PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_RTC PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})

include(GStreamerDefinitions)

# Finalize the value for all options. Do not attempt to use an option before
# this point, and do not attempt to change any option after this point.
WEBKIT_OPTION_END()

SET_AND_EXPOSE_TO_BUILD(WTF_PLATFORM_QUARTZ ${ENABLE_QUARTZ_TARGET})
SET_AND_EXPOSE_TO_BUILD(WTF_PLATFORM_X11 ${ENABLE_X11_TARGET})
SET_AND_EXPOSE_TO_BUILD(WTF_PLATFORM_WAYLAND ${ENABLE_WAYLAND_TARGET})

if (ENABLE_NETSCAPE_PLUGIN_API)
    # MOZ_X11 and XP_UNIX are required by npapi.h. Their value is not checked;
    # only their definedness is. They should only be defined in the true case.
    if (ENABLE_X11_TARGET)
        SET_AND_EXPOSE_TO_BUILD(MOZ_X11 1)
    endif ()
    SET_AND_EXPOSE_TO_BUILD(XP_UNIX 1)
endif ()

set(ENABLE_PLUGIN_PROCESS ${ENABLE_NETSCAPE_PLUGIN_API})

add_definitions(-DBUILDING_GTK__=1)
add_definitions(-DGETTEXT_PACKAGE="WebKit2GTK-${WEBKITGTK_API_VERSION}")
add_definitions(-DWEBKITGTK_API_VERSION_STRING="${WEBKITGTK_API_VERSION}")
add_definitions(-DJSC_GLIB_API_ENABLED)

set(GTK_LIBRARIES ${GTK3_LIBRARIES})
set(GTK_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS})
set(GDK_LIBRARIES ${GDK3_LIBRARIES})
set(GDK_INCLUDE_DIRS ${GDK3_INCLUDE_DIRS})

SET_AND_EXPOSE_TO_BUILD(HAVE_GTK_GESTURES ${GTK3_SUPPORTS_GESTURES})
SET_AND_EXPOSE_TO_BUILD(HAVE_GTK_UNIX_PRINTING ${GTKUnixPrint_FOUND})

set(glib_components gio gio-unix gobject gthread gmodule)
find_package(GLIB 2.36 REQUIRED COMPONENTS ${glib_components})

if (ENABLE_XSLT)
    find_package(LibXslt 1.1.7 REQUIRED)
endif ()

if (ENABLE_ACCELERATED_2D_CANVAS)
    if (GLX_FOUND)
        list(APPEND CAIROGL_COMPONENTS cairo-glx)
    endif ()
    if (EGL_FOUND)
        list(APPEND CAIROGL_COMPONENTS cairo-egl)
    endif ()

    find_package(CairoGL 1.10.2 COMPONENTS ${CAIROGL_COMPONENTS})
    if (NOT CAIROGL_FOUND)
        message(FATAL_ERROR "CairoGL is needed for ENABLE_ACCELERATED_2D_CANVAS")
    endif ()
endif ()

if (ENABLE_BUBBLEWRAP_SANDBOX)
    find_program(BWRAP_EXECUTABLE bwrap)
    if (NOT BWRAP_EXECUTABLE)
        message(FATAL_ERROR "bwrap executable is needed for ENABLE_BUBBLEWRAP_SANDBOX")
    endif ()
    add_definitions(-DBWRAP_EXECUTABLE="${BWRAP_EXECUTABLE}")

    execute_process(
        COMMAND "${BWRAP_EXECUTABLE}" --version
        RESULT_VARIABLE BWRAP_RET
        OUTPUT_VARIABLE BWRAP_OUTPUT
    )
    if (BWRAP_RET)
        message(FATAL_ERROR "Failed to run ${BWRAP_EXECUTABLE}")
    endif ()
    string(REGEX MATCH "([0-9]+.[0-9]+.[0-9]+)" BWRAP_VERSION "${BWRAP_OUTPUT}")
    if (NOT "${BWRAP_VERSION}" VERSION_GREATER_EQUAL "0.3.1")
        message(FATAL_ERROR "bwrap must be >= 0.3.1 but ${BWRAP_VERSION} found")
    endif ()

    find_package(Libseccomp)
    if (NOT LIBSECCOMP_FOUND)
        message(FATAL_ERROR "libseccomp is needed for ENABLE_BUBBLEWRAP_SANDBOX")
    endif ()

    find_program(DBUS_PROXY_EXECUTABLE xdg-dbus-proxy)
    if (NOT DBUS_PROXY_EXECUTABLE)
        message(FATAL_ERROR "xdg-dbus-proxy not found and is needed for ENABLE_BUBBLEWRAP_SANDBOX")
    endif ()
    add_definitions(-DDBUS_PROXY_EXECUTABLE="${DBUS_PROXY_EXECUTABLE}")
endif ()

if (USE_LIBSECRET)
    find_package(Libsecret)
    if (NOT LIBSECRET_FOUND)
        message(FATAL_ERROR "libsecret is needed for USE_LIBSECRET")
    endif ()
endif ()

if (ENABLE_GEOLOCATION)
    find_package(GeoClue2 2.1.5)
    if (NOT GEOCLUE2_FOUND)
        message(FATAL_ERROR "Geoclue is needed for ENABLE_GEOLOCATION.")
    endif ()
endif ()

if (ENABLE_INTROSPECTION)
    find_package(GObjectIntrospection)
    if (NOT INTROSPECTION_FOUND)
        message(FATAL_ERROR "GObjectIntrospection is needed for ENABLE_INTROSPECTION.")
    endif ()
endif ()

if (ENABLE_WEB_CRYPTO)
    find_package(Libtasn1 REQUIRED)
    if (NOT LIBTASN1_FOUND)
        message(FATAL_ERROR "libtasn1 is required to enable Web Crypto API support.")
    endif ()
    if (LIBGCRYPT_VERSION VERSION_LESS 1.7.0)
        message(FATAL_ERROR "libgcrypt 1.7.0 is required to enable Web Crypto API support.")
    endif ()
endif ()

if (ENABLE_WEBDRIVER)
    # WebDriver requires newer versions of GLib and Soup.
    if (PC_GLIB_VERSION VERSION_LESS "2.40")
        message(FATAL_ERROR "GLib 2.40 is required to enable WebDriver support.")
    endif ()
    if (PC_LIBSOUP_VERSION VERSION_LESS "2.48")
        message(FATAL_ERROR "libsoup 2.48 is required to enable WebDriver support.")
    endif ()
endif ()

SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER TRUE)

if (ENABLE_OPENGL)
    # ENABLE_OPENGL is true if either USE_OPENGL or ENABLE_GLES2 is true.
    # But USE_OPENGL is the opposite of ENABLE_GLES2.
    if (ENABLE_GLES2)
        find_package(OpenGLES2 REQUIRED)
        SET_AND_EXPOSE_TO_BUILD(USE_OPENGL_ES TRUE)

        if (NOT EGL_FOUND)
            message(FATAL_ERROR "EGL is needed for ENABLE_GLES2.")
        endif ()
    else ()
        if (NOT OPENGL_FOUND)
            message(FATAL_ERROR "Either OpenGL or OpenGLES2 is needed for ENABLE_OPENGL.")
        endif ()
        SET_AND_EXPOSE_TO_BUILD(USE_OPENGL TRUE)
    endif ()

    if (NOT EGL_FOUND AND (NOT ENABLE_X11_TARGET OR NOT GLX_FOUND))
        message(FATAL_ERROR "Either GLX or EGL is needed for ENABLE_OPENGL.")
    endif ()

    SET_AND_EXPOSE_TO_BUILD(ENABLE_GRAPHICS_CONTEXT_3D TRUE)

    SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER_GL TRUE)

    SET_AND_EXPOSE_TO_BUILD(USE_EGL ${EGL_FOUND})

    if (ENABLE_X11_TARGET AND GLX_FOUND AND USE_OPENGL)
        SET_AND_EXPOSE_TO_BUILD(USE_GLX TRUE)
    endif ()

    SET_AND_EXPOSE_TO_BUILD(USE_COORDINATED_GRAPHICS TRUE)
    SET_AND_EXPOSE_TO_BUILD(USE_COORDINATED_GRAPHICS_THREADED TRUE)
    SET_AND_EXPOSE_TO_BUILD(USE_NICOSIA TRUE)
endif ()

if (ENABLE_PLUGIN_PROCESS_GTK2)
    find_package(GTK2 2.24.10 REQUIRED)
    find_package(GDK2 2.24.10 REQUIRED)
endif ()

if (ENABLE_SPELLCHECK)
    find_package(Enchant)
    if (NOT PC_ENCHANT_FOUND)
        message(FATAL_ERROR "Enchant is needed for ENABLE_SPELLCHECK")
    endif ()
endif ()

if (ENABLE_QUARTZ_TARGET)
    if (NOT GTK3_SUPPORTS_QUARTZ)
        message(FATAL_ERROR "Recompile GTK+ with Quartz backend to use ENABLE_QUARTZ_TARGET")
    endif ()
endif ()

if (ENABLE_X11_TARGET)
    if (NOT GTK3_SUPPORTS_X11)
        message(FATAL_ERROR "Recompile GTK+ with X11 backend to use ENABLE_X11_TARGET")
    endif ()

    find_package(X11 REQUIRED)
    if (NOT X11_Xcomposite_FOUND)
        message(FATAL_ERROR "libXcomposite is required for ENABLE_X11_TARGET")
    elseif (NOT X11_Xdamage_FOUND)
        message(FATAL_ERROR "libXdamage is required for ENABLE_X11_TARGET")
    elseif (NOT X11_Xrender_FOUND)
        message(FATAL_ERROR "libXrender is required for ENABLE_X11_TARGET")
    elseif (NOT X11_Xt_FOUND)
        message(FATAL_ERROR "libXt is required for ENABLE_X11_TARGET")
    endif ()
endif ()

if (ENABLE_WAYLAND_TARGET)
    if (NOT GTK3_SUPPORTS_WAYLAND)
        message(FATAL_ERROR "Recompile GTK+ with Wayland backend to use ENABLE_WAYLAND_TARGET")
    endif ()

    if (GTK3_VERSION VERSION_LESS 3.12)
        message(FATAL_ERROR "GTK+ 3.12 is required to use ENABLE_WAYLAND_TARGET")
    endif ()

    if (NOT EGL_FOUND)
        message(FATAL_ERROR "EGL is required to use ENABLE_WAYLAND_TARGET")
    endif ()

    find_package(Wayland REQUIRED)
endif ()

if (USE_LIBNOTIFY)
    find_package(LibNotify)
    if (NOT LIBNOTIFY_FOUND)
       message(FATAL_ERROR "libnotify is needed for USE_LIBNOTIFY.")
    endif ()
endif ()

if (USE_LIBHYPHEN)
    find_package(Hyphen)
    if (NOT HYPHEN_FOUND)
       message(FATAL_ERROR "libhyphen is needed for USE_LIBHYPHEN.")
    endif ()
endif ()

if (USE_WOFF2)
    find_package(WOFF2Dec 1.0.2)
    if (NOT WOFF2DEC_FOUND)
       message(FATAL_ERROR "libwoff2dec is needed for USE_WOFF2.")
    endif ()
endif ()

# https://bugs.webkit.org/show_bug.cgi?id=182247
if (ENABLED_COMPILER_SANITIZERS)
    set(ENABLE_INTROSPECTION OFF)
endif ()

# Override the cached variables, gtk-doc and gobject-introspection do not really work when cross-building.
if (CMAKE_CROSSCOMPILING)
    set(ENABLE_GTKDOC OFF)
    set(ENABLE_INTROSPECTION OFF)
endif ()

# Override the cached variable, gtk-doc does not really work when building on Mac.
if (APPLE)
    set(ENABLE_GTKDOC OFF)
endif ()

set(DERIVED_SOURCES_WEBKITGTK_DIR ${DERIVED_SOURCES_DIR}/webkitgtk)
set(DERIVED_SOURCES_WEBKITGTK_API_DIR ${DERIVED_SOURCES_WEBKITGTK_DIR}/webkit)
set(DERIVED_SOURCES_WEBKIT2GTK_DIR ${DERIVED_SOURCES_DIR}/webkit2gtk)
set(DERIVED_SOURCES_WEBKIT2GTK_API_DIR ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/webkit2)
set(DERIVED_SOURCES_JAVASCRIPCOREGTK_DIR ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/javascriptcoregtk)
set(DERIVED_SOURCES_JAVASCRIPCORE_GLIB_API_DIR ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/javascriptcoregtk/jsc)
set(FORWARDING_HEADERS_WEBKIT2GTK_DIR ${FORWARDING_HEADERS_DIR}/webkit2gtk)
set(FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR ${FORWARDING_HEADERS_DIR}/webkit2gtk-webextension)

set(JavaScriptCore_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc)
set(WebKit2_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/Source/WebKit/webkit2gtk-${WEBKITGTK_API_VERSION}.pc)
set(WebKit2WebExtension_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/Source/WebKit/webkit2gtk-web-extension-${WEBKITGTK_API_VERSION}.pc)

set(JavaScriptCore_LIBRARY_TYPE SHARED)
set(SHOULD_INSTALL_JS_SHELL ON)

# Add a typelib file to the list of all typelib dependencies. This makes it easy to
# expose a 'gir' target with all gobject-introspection files.
macro(ADD_TYPELIB typelib)
    if (ENABLE_INTROSPECTION)
        get_filename_component(target_name ${typelib} NAME_WE)
        add_custom_target(${target_name}-gir ALL DEPENDS ${typelib})
        list(APPEND GObjectIntrospectionTargets ${target_name}-gir)
        set(GObjectIntrospectionTargets ${GObjectIntrospectionTargets} PARENT_SCOPE)
    endif ()
endmacro()

# CMake does not automatically add --whole-archive when building shared objects from
# a list of convenience libraries. This can lead to missing symbols in the final output.
# We add --whole-archive to all libraries manually to prevent the linker from trimming
# symbols that we actually need later. With ld64 on darwin, we use -all_load instead.
macro(ADD_WHOLE_ARCHIVE_TO_LIBRARIES _list_name)
    if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        list(APPEND ${_list_name} -Wl,-all_load)
    else ()
        set(_tmp)
        foreach (item IN LISTS ${_list_name})
            if ("${item}" STREQUAL "PRIVATE" OR "${item}" STREQUAL "PUBLIC")
                list(APPEND _tmp "${item}")
            else ()
                list(APPEND _tmp -Wl,--whole-archive "${item}" -Wl,--no-whole-archive)
            endif ()
        endforeach ()
        set(${_list_name} ${_tmp})
    endif ()
endmacro()

include(GStreamerChecks)
