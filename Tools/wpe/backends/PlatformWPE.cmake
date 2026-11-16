find_package(XkbCommon 0.4.0 REQUIRED)
find_package(Wayland 1.15 REQUIRED)
find_package(WaylandProtocols 1.15 REQUIRED)
find_package(WPEBackendFDO 1.3.0 REQUIRED)

list(APPEND WPEToolingBackends_PUBLIC_HEADERS
    ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    fdo/WindowViewBackend.h
)

list(APPEND WPEToolingBackends_SOURCES
    ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-protocol.c

    fdo/HeadlessViewBackendFdo.cpp
    fdo/WindowViewBackend.cpp
)

list(APPEND WPEToolingBackends_PRIVATE_INCLUDE_DIRECTORIES
    ${TOOLS_DIR}/wpe/backends/fdo
)

list(APPEND WPEToolingBackends_SYSTEM_INCLUDE_DIRECTORIES
    ${LIBEPOXY_INCLUDE_DIRS}
)

list(APPEND WPEToolingBackends_LIBRARIES
    GLib::Object
    WPE::FDO
    XkbCommon::XkbCommon
    ${LIBEPOXY_LIBRARIES}
    ${WAYLAND_LIBRARIES}
)

list(APPEND WPEToolingBackends_DEFINITIONS USE_GLIB=1)

if (USE_ATK)
    list(APPEND WPEToolingBackends_SOURCES
        atk/ViewBackendAtk.cpp
        atk/WebKitAccessibleApplication.cpp
    )

    list(APPEND WPEToolingBackends_PRIVATE_LIBRARIES
        ATK::ATK
        ATK::Bridge
    )

    list(APPEND WPEToolingBackends_DEFINITIONS USE_ATK=1)
endif ()

list(APPEND WPEToolingBackends_PRIVATE_DEFINITIONS ${LIBEPOXY_DEFINITIONS})

add_custom_command(
    OUTPUT ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
    DEPENDS ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml ${WPEToolingBackends_DERIVED_SOURCES_DIR}/xdg-shell-client-protocol.h
    VERBATIM)

list(APPEND WPEToolingBackends_DEFINITIONS WPE_BACKEND_FDO)
list(APPEND WPEToolingBackends_PRIVATE_DEFINITIONS WPE_BACKEND="libWPEBackend-fdo-1.0.so")

list(APPEND WPEToolingBackends_PRIVATE_DEFINITIONS
    GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_40
)
