configure_file(wpe-platform-drm.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-drm-${WPE_API_VERSION}.pc @ONLY)
configure_file(wpe-platform-drm-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-drm-${WPE_API_VERSION}-uninstalled.pc @ONLY)

set(WPEPlatformDRM_SOURCES
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/RefPtrUdev.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDRM.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDRMCursor.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDRMCursorTheme.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDRMSeat.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDRMSession.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDRMSessionLogind.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDisplayDRM.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEScreenDRM.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEToplevelDRM.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEViewDRM.cpp
)

set(WPEPlatformDRM_INSTALLED_HEADERS
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/wpe-drm.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEDisplayDRM.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEScreenDRM.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEToplevelDRM.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/drm/WPEViewDRM.h
)

set(WPEPlatformDRM_PRIVATE_INCLUDE_DIRECTORIES
    "${WPEPlatform_PRIVATE_INCLUDE_DIRECTORIES}"
    "${WEBKIT_DIR}/WPEPlatform/wpe/drm"
)

set(WPEPlatformDRM_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${WPEPlatform_SYSTEM_INCLUDE_DIRECTORIES}
)

set(WPEPlatformDRM_LIBRARIES
    LibInput::LibInput
    Udev::Udev
    ${GIO_UNIX_LIBRARIES}
)

if (ENABLE_JOURNALD_LOG)
    list(APPEND WPEPlatformDRM_LIBRARIES Journald::Journald)
endif ()

set(WPEPlatformDRM_SOURCES_FOR_INTROSPECTION
    ${WPEPlatformDRM_INSTALLED_HEADERS}
    ${WPEPlatformDRM_SOURCES}
)

add_library(WPEPlatformDRM OBJECT ${WPEPlatformDRM_SOURCES})
add_dependencies(WPEPlatformDRM WPEPlatformGeneratedEnumTypesHeader)
target_include_directories(WPEPlatformDRM PRIVATE ${WPEPlatformDRM_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WPEPlatformDRM SYSTEM PRIVATE ${WPEPlatformDRM_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEPlatformDRM ${WPEPlatform_LIBRARIES} ${WPEPlatformDRM_LIBRARIES})

set_target_properties(WPEPlatformDRM PROPERTIES
    SOURCES_FOR_INTROSPECTION "${WPEPlatformDRM_SOURCES_FOR_INTROSPECTION}"
)

install(FILES "${CMAKE_BINARY_DIR}/wpe-platform-drm-${WPE_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
        COMPONENT "Development"
)

install(FILES ${WPEPlatformDRM_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/wpe-platform/wpe/drm"
        COMPONENT "Development"
)
