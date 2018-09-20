find_package(Libxkbcommon 0.4.0 REQUIRED)
find_package(Wayland REQUIRED)
find_package(WaylandProtocols 1.12 REQUIRED)
find_package(WPEBackend-fdo REQUIRED)

set(DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WPEToolingBackends")

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR})

set(WPEToolingBackends_SOURCES
    ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR}/xdg-shell-unstable-v6-protocol.c
    ${TOOLS_DIR}/wpe/backends/HeadlessViewBackend.cpp
    ${TOOLS_DIR}/wpe/backends/ViewBackend.cpp
    ${TOOLS_DIR}/wpe/backends/WindowViewBackend.cpp
)

set(WPEToolingBackends_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR}
)

set(WPEToolingBackends_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBEPOXY_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
)

set(WPEToolingBackends_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBEPOXY_LIBRARIES}
    ${LIBXKBCOMMON_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WPE_LIBRARIES}
    ${WPEBACKEND_FDO_LIBRARIES}
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR}/xdg-shell-unstable-v6-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml
    DEPENDS ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR}/xdg-shell-unstable-v6-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR}/xdg-shell-unstable-v6-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR}/xdg-shell-unstable-v6-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/xdg-shell-unstable-v6.xml ${DERIVED_SOURCES_WPETOOLINGBACKENDS_DIR}/xdg-shell-unstable-v6-client-protocol.h
    VERBATIM)

add_library(WPEToolingBackends ${WPEToolingBackends_SOURCES})
include_directories(${WPEToolingBackends_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${WPEToolingBackends_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEToolingBackends ${WPEToolingBackends_LIBRARIES})
