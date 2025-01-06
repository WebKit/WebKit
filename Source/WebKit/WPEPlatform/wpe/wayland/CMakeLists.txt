file(MAKE_DIRECTORY ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland)

configure_file(wpe-platform-wayland.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-wayland-${WPE_API_VERSION}.pc @ONLY)
configure_file(wpe-platform-wayland-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-wayland-${WPE_API_VERSION}-uninstalled.pc @ONLY)

set(WPEPlatformWayland_SOURCES
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEDisplayWayland.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEInputMethodContextWaylandV1.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEInputMethodContextWaylandV3.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEScreenWayland.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEToplevelWayland.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEViewWayland.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEWaylandCursor.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEWaylandCursorTheme.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEWaylandSHMPool.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEWaylandSeat.cpp
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-dmabuf-unstable-v1-protocol.c
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-explicit-synchronization-unstable-v1-protocol.c
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v1-protocol.c
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v3-protocol.c
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/xdg-shell-protocol.c
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/pointer-constraints-unstable-v1-protocol.c
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/relative-pointer-unstable-v1-protocol.c
)

set(WPEPlatformWayland_INSTALLED_HEADERS
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/wpe-wayland.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEDisplayWayland.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEScreenWayland.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEToplevelWayland.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/wayland/WPEViewWayland.h
)

set(WPEPlatformWayland_PRIVATE_INCLUDE_DIRECTORIES
    "${WPEPlatform_PRIVATE_INCLUDE_DIRECTORIES}"
    "${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland"
    "${WEBKIT_DIR}/WPEPlatform/wpe/wayland"
)

set(WPEPlatformWayland_SYSTEM_INCLUDE_DIRECTORIES
    ${WAYLAND_INCLUDE_DIRS}
    ${WPEPlatform_SYSTEM_INCLUDE_DIRECTORIES}
)

set(WPEPlatformWayland_LIBRARIES
    ${WAYLAND_LIBRARIES}
    ${WPEPlatform_LIBRARIES}
)

check_function_exists(shm_open SHM_OPEN_EXISTS)
if (NOT SHM_OPEN_EXISTS)
    check_function_exists(shm_open SHM_OPEN_REQUIRES_LIBRT)
    if (SHM_OPEN_REQUIRES_LIBRT)
        list(APPEND WPEPlatformWayland_LIBRARIES rt)
    endif ()
endif ()

set(WPEPlatformWayland_SOURCES_FOR_INTROSPECTION
    ${WPEPlatformWayland_INSTALLED_HEADERS}
    ${WPEPlatformWayland_SOURCES}
)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/xdg-shell-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/xdg-shell-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/xdg-shell-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/xdg-shell-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/xdg-shell-client-protocol.h
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-dmabuf-unstable-v1-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-dmabuf-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-dmabuf-unstable-v1-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-dmabuf-unstable-v1-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-dmabuf-unstable-v1-client-protocol.h
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v1-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v1-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v1-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v1.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v1-client-protocol.h
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v3-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v3-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v3.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v3-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v3-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/text-input/text-input-unstable-v3.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/text-input-unstable-v3-client-protocol.h
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/pointer-constraints-unstable-v1-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/pointer-constraints-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/pointer-constraints-unstable-v1-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/pointer-constraints-unstable-v1-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/pointer-constraints-unstable-v1-client-protocol.h
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/relative-pointer-unstable-v1-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/relative-pointer-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/relative-pointer-unstable-v1-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/relative-pointer-unstable-v1-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/relative-pointer-unstable-v1-client-protocol.h
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-explicit-synchronization-unstable-v1-protocol.c
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-explicit-synchronization/linux-explicit-synchronization-unstable-v1.xml
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-explicit-synchronization-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-explicit-synchronization/linux-explicit-synchronization-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-explicit-synchronization-unstable-v1-protocol.c
    VERBATIM)

add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-explicit-synchronization-unstable-v1-client-protocol.h
    MAIN_DEPENDENCY ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-explicit-synchronization/linux-explicit-synchronization-unstable-v1.xml
    COMMAND ${WAYLAND_SCANNER} client-header ${WAYLAND_PROTOCOLS_DATADIR}/unstable/linux-explicit-synchronization/linux-explicit-synchronization-unstable-v1.xml ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/wayland/linux-explicit-synchronization-unstable-v1-client-protocol.h
    VERBATIM)

add_library(WPEPlatformWayland OBJECT ${WPEPlatformWayland_SOURCES})
add_dependencies(WPEPlatformWayland WPEPlatformGeneratedEnumTypesHeader)
target_include_directories(WPEPlatformWayland PRIVATE ${WPEPlatformWayland_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WPEPlatformWayland SYSTEM PRIVATE ${WPEPlatformWayland_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEPlatformWayland ${WPEPlatformWayland_LIBRARIES})

set_target_properties(WPEPlatformWayland PROPERTIES
    SOURCES_FOR_INTROSPECTION "${WPEPlatformWayland_SOURCES_FOR_INTROSPECTION}"
)

install(FILES "${CMAKE_BINARY_DIR}/wpe-platform-wayland-${WPE_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
        COMPONENT "Development"
)

install(FILES ${WPEPlatformWayland_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/wpe-platform/wpe/wayland"
        COMPONENT "Development"
)
