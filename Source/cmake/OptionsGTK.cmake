include(GNUInstallDirs)

set(PROJECT_VERSION_MAJOR 2)
set(PROJECT_VERSION_MINOR 5)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(WEBKITGTK_API_VERSION 3.0)

# Libtool library version, not to be confused with API version.
# See http://www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html
CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(WEBKIT2 36 0 11)
CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(JAVASCRIPTCORE 17 0 17)

set(ENABLE_CREDENTIAL_STORAGE ON CACHE BOOL "Whether or not to enable support for credential storage using libsecret.")
set(ENABLE_GTKDOC OFF CACHE BOOL "Whether or not to use generate gtkdoc.")

find_package(Cairo 1.10.2 REQUIRED)
find_package(Fontconfig 2.8.0 REQUIRED)
find_package(Freetype2 2.4.2 REQUIRED)
find_package(GTK2 2.24.10 REQUIRED)
find_package(GDK2 2.24.10 REQUIRED)
find_package(HarfBuzz 0.9.2 REQUIRED)
find_package(ICU REQUIRED)
find_package(JPEG REQUIRED)
find_package(LibSoup 2.40.3 REQUIRED)
find_package(LibXml2 2.8.0 REQUIRED)
find_package(LibXslt 1.1.7 REQUIRED)
find_package(PNG REQUIRED)
find_package(Sqlite REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(ATK REQUIRED)
find_package(WebP REQUIRED)
find_package(ATSPI 2.5.3)
find_package(GObjectIntrospection)
find_package(OpenGL)
find_package(EGL)

WEBKIT_OPTION_BEGIN()
if (OPENGL_FOUND AND (GLX_FOUND OR EGL_FOUND))
      WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGL ON)
endif ()

if (DEVELOPER_MODE)
    set(ENABLE_TOOLS ON)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS ON)
else ()
    set(ENABLE_TOOLS OFF)
    WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS OFF)
    set(VERSION_SCRIPT "-Wl,--version-script,${CMAKE_SOURCE_DIR}/Source/autotools/symbols.filter")
endif ()

# FIXME: We want to expose fewer options to downstream, but for now everything is public.
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_3D_RENDERING ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ACCESSIBILITY ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_BATTERY_STATUS OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS3_TEXT ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_DEVICE_ADAPTATION ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_GRID_LAYOUT OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_IMAGE_SET ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_REGIONS ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_CUSTOM_SCHEME_HANDLER OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DATALIST_ELEMENT ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DOWNLOAD_ATTRIBUTE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_DRAG_SUPPORT ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ENCRYPTED_MEDIA OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_ENCRYPTED_MEDIA_V2 OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FILTERS ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_FULLSCREEN_API ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GAMEPAD OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_GAMEPAD_DEPRECATED OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INDEXED_DATABASE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INDEXED_DATABASE_IN_WORKERS OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_COLOR OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_LINK_PREFETCH ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_CAPTURE ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_CONTROLS_SCRIPT ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEMORY_SAMPLER ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_MHTML ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NAVIGATOR_CONTENT_UTILS OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NAVIGATOR_HWCONCURRENCY ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETSCAPE_PLUGIN_API ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_QUOTA OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOLUTION_MEDIA_QUERY OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_REQUEST_ANIMATION_FRAME ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SECCOMP_FILTERS OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SHARED_WORKERS ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_SPELLCHECK ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TEMPLATE_ELEMENT ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_TOUCH_EVENTS ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_USERSELECT_ALL ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIBRATION OFF)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO_TRACK ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIEW_MODE_CSS_MEDIA ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_TIMING ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_XHR_TIMEOUT ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETWORK_PROCESS ON)
WEBKIT_OPTION_DEFAULT_PORT_VALUE(WTF_USE_TILED_BACKING_STORE OFF)
WEBKIT_OPTION_END()

set(ENABLE_X11_TARGET ON CACHE BOOL "Whether to enable support for the X11 windowing target.")
set(ENABLE_WAYLAND_TARGET OFF CACHE BOOL "Whether to enable support for the Wayland windowing target.")

if (ENABLE_X11_TARGET)
    add_definitions(-DWTF_PLATFORM_X11=1)
    add_definitions(-DMOZ_X11=1)
    if (WTF_OS_UNIX)
        add_definitions(-DXP_UNIX)
    endif ()
endif ()

if (ENABLE_WAYLAND_TARGET)
    add_definitions(-DWTF_PLATFORM_WAYLAND=1)
endif ()

set(ENABLE_WEBCORE ON)
set(ENABLE_INSPECTOR ON)
set(ENABLE_WEBKIT OFF)
set(ENABLE_WEBKIT2 ON)
set(ENABLE_PLUGIN_PROCESS ON)

set(GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_6)
set(GTK_API_VERSION 3.0)

set(WTF_USE_SOUP 1)
set(WTF_USE_UDIS86 1)

set(WTF_OUTPUT_NAME WTFGTK)
set(JavaScriptCore_OUTPUT_NAME javascriptcoregtk-${WEBKITGTK_API_VERSION})
set(WebCore_OUTPUT_NAME WebCoreGTK)
set(WebKit_OUTPUT_NAME webkitgtk-${WEBKITGTK_API_VERSION})
set(WebKit2_OUTPUT_NAME webkit2gtk-${WEBKITGTK_API_VERSION})
set(WebKit2_WebProcess_OUTPUT_NAME WebKitWebProcess)
set(WebKit2_NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(WebKit2_PluginProcess_OUTPUT_NAME WebKitPluginProcess)

# These are shared variables, but we special case their definition so that we can use the
# CMAKE_INSTALL_* variables that are populated by the GNUInstallDirs macro.
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE PATH "Absolute path to library installation directory")
set(EXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE PATH "Absolute path to executable installation directory")
set(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}" CACHE PATH "Absolute path to install executables executed by the library")

set(DATA_BUILD_DIR "${CMAKE_BINARY_DIR}/share/${WebKit_OUTPUT_NAME}")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/webkitgtk-${WEBKITGTK_API_VERSION}")
set(WEBKITGTK_HEADER_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/webkitgtk-${WEBKITGTK_API_VERSION}")

add_definitions(-DBUILDING_GTK__=1)
add_definitions(-DGETTEXT_PACKAGE="WebKit2GTK-${WEBKITGTK_API_VERSION}")
add_definitions(-DDATA_DIR="${CMAKE_INSTALL_DATADIR}")
add_definitions(-DUSER_AGENT_GTK_MAJOR_VERSION=538)
add_definitions(-DUSER_AGENT_GTK_MINOR_VERSION=35)
add_definitions(-DWEBKITGTK_API_VERSION_STRING="${WEBKITGTK_API_VERSION}")

if (ENABLE_VIDEO OR ENABLE_WEB_AUDIO)
    set(GSTREAMER_COMPONENTS app pbutils)
    add_definitions(-DWTF_USE_GSTREAMER)
    if (ENABLE_VIDEO)
        list(APPEND GSTREAMER_COMPONENTS video mpegts tag)
    endif ()

    if (ENABLE_WEB_AUDIO)
        list(APPEND GSTREAMER_COMPONENTS audio fft)
        add_definitions(-DWTF_USE_WEBAUDIO_GSTREAMER)
    endif ()

    find_package(GStreamer 1.0.3 REQUIRED COMPONENTS ${GSTREAMER_COMPONENTS})

    if (PC_GSTREAMER_MPEGTS_FOUND)
        add_definitions(-DWTF_USE_GSTREAMER_MPEGTS)
        set(USE_GSTREAMER_MPEGTS TRUE)
    endif ()
endif ()

if (ENABLE_WAYLAND_TARGET)
    set(GTK3_REQUIRED_VERSION 3.12.0)
else ()
    set(GTK3_REQUIRED_VERSION 3.6.0)
endif ()

find_package(GTK3 ${GTK3_REQUIRED_VERSION} REQUIRED)
find_package(GDK3 ${GTK3_REQUIRED_VERSION} REQUIRED)
set(GTK_LIBRARIES ${GTK3_LIBRARIES})
set(GTK_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS})
set(GDK_LIBRARIES ${GDK3_LIBRARIES})
set(GDK_INCLUDE_DIRS ${GDK3_INCLUDE_DIRS})

set(glib_components gio gobject gthread gmodule)
if (ENABLE_GAMEPAD_DEPRECATED OR ENABLE_GEOLOCATION)
    list(APPEND glib_components gio-unix)
endif ()
find_package(GLIB 2.33.2 REQUIRED COMPONENTS ${glib_components})

if (ENABLE_GEOLOCATION)
    find_package(GeoClue2 2.1.5)
    if (GEOCLUE2_FOUND)
      set(WTF_USE_GEOCLUE2 1)
    else ()
      find_package(GeoClue)
      set(WTF_USE_GEOCLUE2 0)
    endif ()
endif ()

find_package(GTKUnixPrint)
if (GTK_UNIX_PRINT_FOUND)
    set(HAVE_GTK_UNIX_PRINTING 1)
endif ()

if (ENABLE_CREDENTIAL_STORAGE)
    find_package(Libsecret)
    set(ENABLE_CREDENTIAL_STORAGE 1)
endif ()

# This part can be simplified once CMake 2.8.6 is required and
# CMakePushCheckState can be used. We need to have OPENGL_INCLUDE_DIR as part
# of the directories check_include_files() looks for in case OpenGL is
# installed into a non-standard location.
if (ENABLE_X11_TARGET)
    set(REQUIRED_INCLUDES_OLD ${CMAKE_REQUIRED_INCLUDES})
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${OPENGL_INCLUDE_DIR})
    # We don't use find_package for GLX because it is part of -lGL, unlike EGL.
    check_include_files("GL/glx.h" GLX_FOUND)
    set(CMAKE_REQUIRED_INCLUDES ${REQUIRED_INCLUDES_OLD})

    if (GLX_FOUND)
        set(WTF_USE_GLX 1)
    endif ()
endif ()

if (EGL_FOUND)
    set(WTF_USE_EGL 1)
endif ()

if (ENABLE_SPELLCHECK)
    find_package(Enchant REQUIRED)
endif ()

if (OPENGL_FOUND AND (GLX_FOUND OR EGL_FOUND))
    set(ENABLE_TEXTURE_MAPPER 1)
    set(WTF_USE_3D_GRAPHICS 1)

    add_definitions(-DWTF_USE_OPENGL=1)
    add_definitions(-DWTF_USE_ACCELERATED_COMPOSITING=1)
    add_definitions(-DWTF_USE_3D_GRAPHICS=1)
    add_definitions(-DWTF_USE_TEXTURE_MAPPER=1)
    add_definitions(-DWTF_USE_TEXTURE_MAPPER_GL=1)
    add_definitions(-DENABLE_3D_RENDERING=1)

    if (EGL_FOUND)
        add_definitions(-DWTF_USE_EGL=1)
    endif ()

    if (GLX_FOUND)
        add_definitions(-DWTF_USE_GLX=1)
    endif ()
endif ()

if (ENABLE_INDEXED_DATABASE)
    set(WTF_USE_LEVELDB 1)
    add_definitions(-DWTF_USE_LEVELDB=1)
endif ()

if (ENABLE_GAMEPAD_DEPRECATED)
    find_package(GUdev)
endif ()

if (ENABLE_FTL_JIT)
    find_package(LLVM REQUIRED)
    find_package(LIBCXXABI REQUIRED)
    set(HAVE_LLVM ON)
endif ()

set(CPACK_SOURCE_GENERATOR TBZ2)

set(DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR ${DERIVED_SOURCES_DIR}/webkitdom)
set(DERIVED_SOURCES_WEBKITGTK_DIR ${DERIVED_SOURCES_DIR}/webkitgtk)
set(DERIVED_SOURCES_WEBKITGTK_API_DIR ${DERIVED_SOURCES_WEBKITGTK_DIR}/webkit)
set(DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR ${DERIVED_SOURCES_DIR}/webkitdom)
set(DERIVED_SOURCES_WEBKIT2GTK_DIR ${DERIVED_SOURCES_DIR}/webkit2gtk)
set(DERIVED_SOURCES_WEBKIT2GTK_API_DIR ${DERIVED_SOURCES_WEBKIT2GTK_DIR}/webkit2)
set(FORWARDING_HEADERS_DIR ${DERIVED_SOURCES_DIR}/ForwardingHeaders)
set(FORWARDING_HEADERS_WEBKIT2GTK_DIR ${FORWARDING_HEADERS_DIR}/webkit2gtk)
set(FORWARDING_HEADERS_WEBKIT2GTK_EXTENSION_DIR ${FORWARDING_HEADERS_DIR}/webkit2gtk-webextension)

set(WebKit_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/Source/WebKit/gtk/webkitgtk-${WEBKITGTK_API_VERSION}.pc)
set(WebKit2_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/Source/WebKit2/webkit2gtk-${WEBKITGTK_API_VERSION}.pc)
set(WebKit2WebExtension_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/Source/WebKit2/webkit2gtk-web-extension-${WEBKITGTK_API_VERSION}.pc)

set(SHOULD_INSTALL_JS_SHELL ON)

# Push of rbp is needed after JSC JIT uses CStack. See http://wkbug.com/127777.
if (UNIX AND NOT APPLE)
    if (CMAKE_COMPILER_IS_GNUCXX)
        set(CMAKE_C_FLAGS_RELEASE "-fno-omit-frame-pointer -fno-tree-dce ${CMAKE_C_FLAGS_RELEASE}")
        set(CMAKE_CXX_FLAGS_RELEASE "-fno-omit-frame-pointer -fno-tree-dce ${CMAKE_CXX_FLAGS_RELEASE}")
    endif ()
    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
        set(CMAKE_C_FLAGS_RELEASE "-fno-omit-frame-pointer ${CMAKE_C_FLAGS_RELEASE}")
        set(CMAKE_CXX_FLAGS_RELEASE "-fno-omit-frame-pointer ${CMAKE_CXX_FLAGS_RELEASE}")
    endif ()
endif ()

# Add a typelib file to the list of all typelib dependencies. This makes it easy to
# expose a 'gir' target with all gobject-introspection files.
macro(ADD_TYPELIB typelib)
    get_filename_component(target_name ${typelib} NAME_WE)
    add_custom_target(${target_name}-gir ALL DEPENDS ${typelib})
    list(APPEND GObjectIntrospectionTargets ${target_name}-gir)
    set(GObjectIntrospectionTargets ${GObjectIntrospectionTargets} PARENT_SCOPE)
endmacro()

# CMake does not automatically add --whole-archive when building shared objects from
# a list of convenience libraries. This can lead to missing symbols in the final output.
# We add --whole-archive to all libraries manually to prevent the linker from trimming
# symbols that we actually need later.
macro(ADD_WHOLE_ARCHIVE_TO_LIBRARIES _list_name)
    foreach (library IN LISTS ${_list_name})
      list(APPEND ${_list_name}_TMP -Wl,--whole-archive ${library} -Wl,--no-whole-archive)
    endforeach ()
    set(${_list_name} "${${_list_name}_TMP}")
endmacro()

build_command(COMMAND_LINE_TO_BUILD)
# build_command unconditionally adds -i (ignore errors) for make, and there's
# no reasonable way to turn that off, so we just replace it with -k, which has
# the same effect, except that the return code will indicate that an error occurred.
# See: http://www.cmake.org/cmake/help/v3.0/command/build_command.html
string(REPLACE " -i" " -k" COMMAND_LINE_TO_BUILD ${COMMAND_LINE_TO_BUILD})
file(WRITE
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/build.sh
    "#!/bin/sh\n"
    "${COMMAND_LINE_TO_BUILD} $@"
)
file(COPY ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/build.sh
  DESTINATION ${CMAKE_BINARY_DIR}
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE)
