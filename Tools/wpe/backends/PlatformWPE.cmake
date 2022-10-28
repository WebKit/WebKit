find_package(Libxkbcommon 0.4.0 REQUIRED)
find_package(Wayland REQUIRED)
find_package(WaylandProtocols 1.12 REQUIRED)
find_package(WPEBackend_fdo 1.3.0 REQUIRED)

list(APPEND WPEToolingBackends_PUBLIC_HEADERS
    ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-unstable-v6-client-protocol.h
    fdo/WindowViewBackend.h
)

list(APPEND WPEToolingBackends_SOURCES
    ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-protocol.c
    ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-unstable-v6-protocol.c

    atk/ViewBackendAtk.cpp
    atk/WebKitAccessibleApplication.cpp

    fdo/HeadlessViewBackendFdo.cpp
    fdo/WindowViewBackend.cpp
)

list(APPEND WPEToolingBackends_PRIVATE_INCLUDE_DIRECTORIES
    ${TOOLS_DIR}/wpe/backends/atk
    ${TOOLS_DIR}/wpe/backends/fdo
)

list(APPEND WPEToolingBackends_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBEPOXY_INCLUDE_DIRS}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
)

list(APPEND WPEToolingBackends_LIBRARIES
    ${ATK_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBEPOXY_LIBRARIES}
    ${LIBXKBCOMMON_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WPEBACKEND_FDO_LIBRARIES}
)

list(APPEND WPEToolingBackends_DEFINITIONS USE_GLIB=1)
list(APPEND WPEToolingBackends_PRIVATE_DEFINITIONS ${LIBEPOXY_DEFINITIONS})

add_custom_command(
    OUTPUT ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
    DEPENDS ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} code ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-unstable-v6-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml
    DEPENDS ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-unstable-v6-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-unstable-v6-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-unstable-v6-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-unstable-v6-client-protocol.h
    VERBATIM)

list(APPEND WPEToolingBackends_DEFINITIONS WPE_BACKEND_FDO)
list(APPEND WPEToolingBackends_PRIVATE_DEFINITIONS WPE_BACKEND="libWPEBackend-fdo-1.0.so")

if (ENABLE_ACCESSIBILITY)
    list(APPEND WPEToolingBackends_DEFINITIONS ENABLE_ACCESSIBILITY=1)
    list(APPEND WPEToolingBackends_PRIVATE_DEFINITIONS
        GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_40
    )
    list(APPEND WPEToolingBackends_LIBRARIES ATK::Bridge)
endif ()
