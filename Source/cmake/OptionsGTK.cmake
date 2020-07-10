include(GNUInstallDirs)
include(VersioningUtils)

WEBKIT_OPTION_BEGIN()
WEBKIT_OPTION_DEFINE(USE_GTK4 "Whether to enable usage of GTK4 instead of GTK3." PUBLIC OFF)

SET_PROJECT_VERSION(2 29 3)

if (USE_GTK4)
    set(WEBKITGTK_API_VERSION 5.0)
    set(GTK_MINIMUM_VERSION 3.98.5)
    CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(WEBKIT 0 0 0)
else ()
    set(WEBKITGTK_API_VERSION 4.0)
    set(GTK_MINIMUM_VERSION 3.22.0)
    CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(WEBKIT 85 0 48)
endif ()

CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(JAVASCRIPTCORE 35 3 17)

# These are shared variables, but we special case their definition so that we can use the
# CMAKE_INSTALL_* variables that are populated by the GNUInstallDirs macro.
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE PATH "Absolute path to library installation directory")
set(EXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE PATH "Absolute path to executable installation directory")
set(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/webkit2gtk-${WEBKITGTK_API_VERSION}" CACHE PATH "Absolute path to install executables executed by the library")

set(WEBKITGTK_HEADER_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/webkitgtk-${WEBKITGTK_API_VERSION}")
set(INTROSPECTION_INSTALL_GIRDIR "${CMAKE_INSTALL_FULL_DATADIR}/gir-1.0")
set(INTROSPECTION_INSTALL_TYPELIBDIR "${LIB_INSTALL_DIR}/girepository-1.0")

set(USER_AGENT_BRANDING "" CACHE STRING "Branding to add to user agent string")
if (USER_AGENT_BRANDING)
    add_definitions(-DUSER_AGENT_BRANDING="${USER_AGENT_BRANDING}")
endif ()

find_package(Cairo 1.14.0 REQUIRED)
find_package(Fontconfig 2.8.0 REQUIRED)
find_package(Freetype 2.4.2 REQUIRED)
find_package(LibGcrypt 1.6.0 REQUIRED)
find_package(GLIB 2.44.0 REQUIRED COMPONENTS gio gio-unix gobject gthread gmodule)
find_package(GTK ${GTK_MINIMUM_VERSION} REQUIRED OPTIONAL_COMPONENTS unix-print)
find_package(HarfBuzz 0.9.18 REQUIRED COMPONENTS ICU)
find_package(ICU 60.2 REQUIRED COMPONENTS data i18n uc)
find_package(JPEG REQUIRED)
find_package(LibSoup 2.54.0 REQUIRED)
find_package(LibXml2 2.8.0 REQUIRED)
find_package(PNG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(ATK 2.16.0 REQUIRED)
find_package(WebP REQUIRED COMPONENTS demux)
find_package(ATSPI 2.5.3)
find_package(EGL)
find_package(OpenGL)
find_package(OpenGLES2)

include(GStreamerDefinitions)

SET_AND_EXPOSE_TO_BUILD(USE_ATK TRUE)
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
# the feature is by default always on (ENABLE_GRAPHICS_CONTEXT_GL=ON).
# What we select here automatically is if we use OPENGL (ENABLE_GLES2=OFF)
# or OPENGLES2 (ENABLE_GLES2=ON) for building the feature.
set(ENABLE_GLES2_DEFAULT OFF)

if (NOT OPENGL_FOUND AND OPENGLES2_FOUND)
    set(ENABLE_GLES2_DEFAULT ON)
endif ()

# Public options specific to the GTK port. Do not add any options here unless
# there is a strong reason we should support changing the value of the option,
# and the option is not relevant to any other WebKit ports.
WEBKIT_OPTION_DEFINE(ENABLE_GLES2 "Whether to enable OpenGL ES 2.0." PUBLIC ${ENABLE_GLES2_DEFAULT})
WEBKIT_OPTION_DEFINE(ENABLE_GTKDOC "Whether or not to use generate gtkdoc." PUBLIC OFF)
WEBKIT_OPTION_DEFINE(ENABLE_INTROSPECTION "Whether to enable GObject introspection." PUBLIC ON)
WEBKIT_OPTION_DEFINE(ENABLE_GRAPHICS_CONTEXT_GL "Whether to use OpenGL." PUBLIC ON)
WEBKIT_OPTION_DEFINE(ENABLE_QUARTZ_TARGET "Whether to enable support for the Quartz windowing target." PUBLIC ${GTK_SUPPORTS_QUARTZ})
WEBKIT_OPTION_DEFINE(ENABLE_X11_TARGET "Whether to enable support for the X11 windowing target." PUBLIC ${GTK_SUPPORTS_X11})
WEBKIT_OPTION_DEFINE(ENABLE_WAYLAND_TARGET "Whether to enable support for the Wayland windowing target." PUBLIC ${GTK_SUPPORTS_WAYLAND})
WEBKIT_OPTION_DEFINE(USE_LIBNOTIFY "Whether to enable the default web notification implementation." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_LIBHYPHEN "Whether to enable the default automatic hyphenation implementation." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_LIBSECRET "Whether to enable the persistent credential storage using libsecret." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_OPENJPEG "Whether to enable support for JPEG2000 images." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_WOFF2 "Whether to enable support for WOFF2 Web Fonts." PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_WPE_RENDERER "Whether to enable WPE rendering" PUBLIC ON)
WEBKIT_OPTION_DEFINE(USE_SYSTEMD "Whether to enable journald logging" PUBLIC ON)

# Private options specific to the GTK port. Changing these options is
# completely unsupported. They are intended for use only by WebKit developers.
WEBKIT_OPTION_DEFINE(USE_ANGLE_WEBGL "Whether to use ANGLE as WebGL backend." PRIVATE OFF)

# FIXME: Can we use cairo-glesv2 to avoid this conflict?
WEBKIT_OPTION_CONFLICT(ENABLE_ACCELERATED_2D_CANVAS ENABLE_GLES2)

WEBKIT_OPTION_DEPEND(ENABLE_3D_TRANSFORMS ENABLE_GRAPHICS_CONTEXT_GL)
WEBKIT_OPTION_DEPEND(ENABLE_ACCELERATED_2D_CANVAS ENABLE_GRAPHICS_CONTEXT_GL)
WEBKIT_OPTION_DEPEND(ENABLE_ASYNC_SCROLLING ENABLE_GRAPHICS_CONTEXT_GL)
WEBKIT_OPTION_DEPEND(ENABLE_GLES2 ENABLE_GRAPHICS_CONTEXT_GL)
WEBKIT_OPTION_DEPEND(ENABLE_WEBGL ENABLE_GRAPHICS_CONTEXT_GL)
WEBKIT_OPTION_DEPEND(USE_ANGLE_WEBGL ENABLE_WEBGL)
WEBKIT_OPTION_DEPEND(USE_WPE_RENDERER ENABLE_GRAPHICS_CONTEXT_GL)
WEBKIT_OPTION_DEPEND(USE_WPE_RENDERER ENABLE_WAYLAND_TARGET)

SET_AND_EXPOSE_TO_BUILD(ENABLE_DEVELOPER_MODE ${DEVELOPER_MODE})
if (DEVELOPER_MODE)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MINIBROWSER PUBLIC ON)
    if (USE_GTK4)
        WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE OFF)
    else ()
        WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE ON)
    endif ()
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
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_BUBBLEWRAP_SANDBOX PUBLIC OFF)
endif ()

# Enable variation fonts when cairo >= 1.16, fontconfig >= 2.13.0, freetype >= 2.9.0 and harfbuzz >= 1.4.2.
if (("${PC_CAIRO_VERSION}" VERSION_GREATER "1.16.0" OR "${PC_CAIRO_VERSION}" STREQUAL "1.16.0")
    AND ("${PC_FONTCONFIG_VERSION}" VERSION_GREATER "2.13.0" OR "${PC_FONTCONFIG_VERSION}" STREQUAL "2.13.0")
    AND ("${FREETYPE_VERSION_STRING}" VERSION_GREATER "2.9.0" OR "${FREETYPE_VERSION_STRING}" STREQUAL "2.9.0")
    AND ("${PC_HARFBUZZ_VERSION}" VERSION_GREATER "1.4.2" OR "${PC_HARFBUZZ_VERSION}" STREQUAL "1.4.2"))
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VARIATION_FONTS PRIVATE ON)
endif ()

# Public options shared with other WebKit ports. Do not add any options here
# without approval from a GTK reviewer. There must be strong reason to support
# changing the value of the option.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ACCELERATED_2D_CANVAS PUBLIC OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DRAG_SUPPORT PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SPELLCHECK PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TOUCH_EVENTS PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_CRYPTO PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER PUBLIC ON)

# Private options shared with other WebKit ports. Add options here when
# we need a value different from the default defined in WebKitFeatures.cmake.
# Changing these options is completely unsupported.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_AUTOCAPITALIZE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTENT_EXTENSIONS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_CONIC_GRADIENTS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_PAINTING_API PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_TYPED_OM PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CURSOR_VISIBILITY PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DARK_MODE_CSS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DATALIST_ELEMENT PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DOWNLOAD_ATTRIBUTE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ENCRYPTED_MEDIA PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTPDIR PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GAMEPAD PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GPU_PROCESS PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_COLOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LAYOUT_FORMATTING_CONTEXT PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_STREAM PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MHTML PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MOUSE_CURSOR_SCALE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETWORK_CACHE_SPECULATIVE_REVALIDATION PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETWORK_CACHE_STALE_WHILE_REVALIDATE PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_OFFSCREEN_CANVAS PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_THUNDER PRIVATE OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_PERIODIC_MEMORY_MONITOR PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_POINTER_LOCK PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_LOAD_STATISTICS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_WORKER PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SHAREABLE_RESOURCE PUBLIC ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_API_STATISTICS PRIVATE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_RTC PRIVATE ${ENABLE_EXPERIMENTAL_FEATURES})

if (USE_GTK4)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETSCAPE_PLUGIN_API PRIVATE OFF)
endif ()

include(GStreamerDependencies)

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

SET_AND_EXPOSE_TO_BUILD(ENABLE_PLUGIN_PROCESS ${ENABLE_NETSCAPE_PLUGIN_API})

add_definitions(-DBUILDING_GTK__=1)
add_definitions(-DGETTEXT_PACKAGE="WebKit2GTK-${WEBKITGTK_API_VERSION}")
add_definitions(-DWEBKITGTK_API_VERSION_STRING="${WEBKITGTK_API_VERSION}")
add_definitions(-DJSC_GLIB_API_ENABLED)

if (EXISTS "${TOOLS_DIR}/glib/svn-revision")
    execute_process(COMMAND ${TOOLS_DIR}/glib/svn-revision ERROR_QUIET OUTPUT_VARIABLE SVN_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE)
else ()
    set(SVN_REVISION "tarball")
endif ()
add_definitions(-DSVN_REVISION="${SVN_REVISION}")

SET_AND_EXPOSE_TO_BUILD(HAVE_GTK_UNIX_PRINTING ${GTK_UNIX_PRINT_FOUND})

if (USE_WPE_RENDERER)
    find_package(WPE 1.3.0)
    if (NOT WPE_FOUND)
        message(FATAL_ERROR "libwpe is required for USE_WPE_RENDERER")
    endif ()

    find_package(WPEBackend_fdo 1.3.1)
    if (NOT WPEBACKEND_FDO_FOUND)
        message(FATAL_ERROR "WPEBackend-fdo is required for USE_WPE_RENDERER")
    endif ()
endif ()

if (ENABLE_GAMEPAD)
    find_package(Manette 0.2.4)
    if (NOT Manette_FOUND)
        message(FATAL_ERROR "libmanette is required for ENABLE_GAMEPAD")
    endif ()
    SET_AND_EXPOSE_TO_BUILD(USE_MANETTE TRUE)
endif ()

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

if (USE_LIBSECRET)
    find_package(Libsecret)
    if (NOT LIBSECRET_FOUND)
        message(FATAL_ERROR "libsecret is needed for USE_LIBSECRET")
    endif ()
endif ()

if (USE_GTK4)
    set(ENABLE_INTROSPECTION OFF)
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
    SET_AND_EXPOSE_TO_BUILD(ENABLE_WEBDRIVER_KEYBOARD_INTERACTIONS ON)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_WEBDRIVER_MOUSE_INTERACTIONS ON)
    SET_AND_EXPOSE_TO_BUILD(ENABLE_WEBDRIVER_TOUCH_INTERACTIONS OFF)
endif ()

SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER TRUE)

if (ENABLE_GRAPHICS_CONTEXT_GL)
    # ENABLE_GRAPHICS_CONTEXT_GL is true if either USE_OPENGL or ENABLE_GLES2 is true.
    # But USE_OPENGL is the opposite of ENABLE_GLES2.
    if (ENABLE_GLES2)
        find_package(OpenGLES2 REQUIRED)
        SET_AND_EXPOSE_TO_BUILD(USE_OPENGL_ES TRUE)
        SET_AND_EXPOSE_TO_BUILD(USE_OPENGL FALSE)

        if (NOT EGL_FOUND)
            message(FATAL_ERROR "EGL is needed for ENABLE_GLES2.")
        endif ()
    else ()
        if (NOT OPENGL_FOUND)
            message(FATAL_ERROR "Either OpenGL or OpenGLES2 is needed for ENABLE_GRAPHICS_CONTEXT_GL.")
        endif ()
        SET_AND_EXPOSE_TO_BUILD(USE_OPENGL TRUE)
    endif ()

    if (NOT EGL_FOUND AND (NOT ENABLE_X11_TARGET OR NOT GLX_FOUND))
        message(FATAL_ERROR "Either GLX or EGL is needed for ENABLE_GRAPHICS_CONTEXT_GL.")
    endif ()

    SET_AND_EXPOSE_TO_BUILD(USE_TEXTURE_MAPPER_GL TRUE)

    SET_AND_EXPOSE_TO_BUILD(USE_EGL ${EGL_FOUND})

    if (ENABLE_X11_TARGET AND GLX_FOUND AND USE_OPENGL)
        SET_AND_EXPOSE_TO_BUILD(USE_GLX TRUE)
    endif ()

    SET_AND_EXPOSE_TO_BUILD(USE_COORDINATED_GRAPHICS TRUE)
    SET_AND_EXPOSE_TO_BUILD(USE_NICOSIA TRUE)
endif ()

if (USE_ANGLE_WEBGL)
    SET_AND_EXPOSE_TO_BUILD(USE_ANGLE TRUE)
    SET_AND_EXPOSE_TO_BUILD(USE_ANGLE_EGL TRUE)
endif ()

if (ENABLE_SPELLCHECK)
    find_package(Enchant)
    if (NOT PC_ENCHANT_FOUND)
        message(FATAL_ERROR "Enchant is needed for ENABLE_SPELLCHECK")
    endif ()
endif ()

if (ENABLE_QUARTZ_TARGET)
    if (NOT GTK_SUPPORTS_QUARTZ)
        message(FATAL_ERROR "Recompile GTK with Quartz backend to use ENABLE_QUARTZ_TARGET")
    endif ()
endif ()

if (ENABLE_X11_TARGET)
    if (NOT GTK_SUPPORTS_X11)
        message(FATAL_ERROR "Recompile GTK with X11 backend to use ENABLE_X11_TARGET")
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
    if (NOT GTK_SUPPORTS_WAYLAND)
        message(FATAL_ERROR "Recompile GTK with Wayland backend to use ENABLE_WAYLAND_TARGET")
    endif ()

    if (NOT EGL_FOUND)
        message(FATAL_ERROR "EGL is required to use ENABLE_WAYLAND_TARGET")
    endif ()

    find_package(Wayland REQUIRED)
    find_package(WaylandProtocols 1.12 REQUIRED)
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

if (USE_OPENJPEG)
    find_package(OpenJPEG 2.2.0)
    if (NOT OpenJPEG_FOUND)
        message(FATAL_ERROR "libopenjpeg 2.2.0 is required for USE_OPENJPEG.")
    endif ()
endif ()

if (USE_WOFF2)
    find_package(WOFF2 1.0.2 COMPONENTS dec)
    if (NOT WOFF2_FOUND)
       message(FATAL_ERROR "libwoff2dec is needed for USE_WOFF2.")
    endif ()
endif ()

if (USE_SYSTEMD)
    find_package(Systemd)
    if (Systemd_FOUND)
        message(STATUS "Release logs will be sent to the Systemd journal")
        SET_AND_EXPOSE_TO_BUILD(USE_JOURNALD TRUE)
    else ()
        message(FATAL_ERROR "libsystemd is needed for USE_SYSTEMD")
    endif ()
endif ()

if (ENABLE_ENCRYPTED_MEDIA AND ENABLE_THUNDER)
  find_package(Thunder REQUIRED)
endif ()

# https://bugs.webkit.org/show_bug.cgi?id=182247
if (ENABLED_COMPILER_SANITIZERS)
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
set(FORWARDING_HEADERS_WEBKIT2GTK_VERSIONED_DIR ${FORWARDING_HEADERS_DIR}/webkit2gtk-${WEBKITGTK_API_VERSION})
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

include(BubblewrapSandboxChecks)
include(GStreamerChecks)
