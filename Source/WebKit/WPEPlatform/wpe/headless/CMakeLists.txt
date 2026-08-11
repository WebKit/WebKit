configure_file(wpe-platform-headless.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-headless-${WPE_API_VERSION}.pc @ONLY)
configure_file(wpe-platform-headless-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-headless-${WPE_API_VERSION}-uninstalled.pc @ONLY)

set(WPEPlatformHeadless_SOURCES
    ${WEBKIT_DIR}/WPEPlatform/wpe/headless/WPEDisplayHeadless.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/headless/WPEToplevelHeadless.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/headless/WPEViewHeadless.cpp
)

set(WPEPlatformHeadless_INSTALLED_HEADERS
    ${WEBKIT_DIR}/WPEPlatform/wpe/headless/wpe-headless.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/headless/WPEDisplayHeadless.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/headless/WPEToplevelHeadless.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/headless/WPEViewHeadless.h
)

set(WPEPlatformHeadless_PRIVATE_INCLUDE_DIRECTORIES
    "${WPEPlatform_PRIVATE_INCLUDE_DIRECTORIES}"
    "${WEBKIT_DIR}/WPEPlatform/wpe/headless"
)

set(WPEPlatformHeadless_SOURCES_FOR_INTROSPECTION
    ${WPEPlatformHeadless_INSTALLED_HEADERS}
    ${WPEPlatformHeadless_SOURCES}
)

add_library(WPEPlatformHeadless OBJECT ${WPEPlatformHeadless_SOURCES})
target_compile_options(WPEPlatformHeadless PRIVATE ${DEVELOPER_MODE_CXX_FLAGS})
target_include_directories(WPEPlatformHeadless PRIVATE ${WPEPlatformHeadless_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WPEPlatformHeadless SYSTEM PRIVATE ${WPEPlatform_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEPlatformHeadless ${WPEPlatform_LIBRARIES})
add_dependencies(WPEPlatformHeadless WPEPlatformGeneratedEnumTypesHeader)

set_target_properties(WPEPlatformHeadless PROPERTIES
    SOURCES_FOR_INTROSPECTION "${WPEPlatformHeadless_SOURCES_FOR_INTROSPECTION}"
)

install(FILES "${CMAKE_BINARY_DIR}/wpe-platform-headless-${WPE_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
        COMPONENT "Development"
)

install(FILES ${WPEPlatformHeadless_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/wpe-platform/wpe/headless"
        COMPONENT "Development"
)
