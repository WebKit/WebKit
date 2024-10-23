set_property(DIRECTORY . PROPERTY FOLDER "WPEPlatform")

set(WPE_PLATFORM_MODULE_DIR ${LIB_INSTALL_DIR}/wpe-platform-${WPE_API_VERSION}/modules)
add_definitions(-DWPE_PLATFORM_MODULE_DIR="${WPE_PLATFORM_MODULE_DIR}")

file(MAKE_DIRECTORY ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe)

configure_file(wpe-platform.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-${WPE_API_VERSION}.pc @ONLY)
configure_file(wpe-platform-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-platform-${WPE_API_VERSION}-uninstalled.pc @ONLY)
configure_file(wpe/WPEConfig.h.in ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEConfig.h)
configure_file(wpe/WPEVersion.h.in ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEVersion.h)

set(WPEPlatform_SOURCES
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEEvent.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBuffer.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBufferDMABuf.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBufferDMABufFormats.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBufferSHM.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEColor.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPECursorTheme.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEDisplay.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEEGLError.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEExtensions.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEInputMethodContext.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEInputMethodContextNone.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEKeyUnicode.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEKeymap.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEKeymapXKB.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEGestureController.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEGestureControllerImpl.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEGestureDetector.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPERectangle.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEScreen.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEToplevel.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEVersion.cpp
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEView.cpp
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.cpp
)

set(WPEPlatform_INSTALLED_HEADERS
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEEvent.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBuffer.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBufferDMABuf.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBufferDMABufFormats.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEBufferSHM.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEColor.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEDefines.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEDisplay.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEEGLError.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEInputMethodContext.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEKeyUnicode.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEKeymap.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEKeymapXKB.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEKeysyms.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEGestureController.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPERectangle.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEScreen.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEToplevel.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/WPEView.h
    ${WEBKIT_DIR}/WPEPlatform/wpe/wpe-platform.h
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEConfig.h
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.h
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEVersion.h
)

set(WPEPlatform_ENUM_GENERATION_HEADERS ${WPEPlatform_INSTALLED_HEADERS})
list(REMOVE_ITEM WPEPlatform_ENUM_GENERATION_HEADERS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.h)
add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.h
    DEPENDS ${WPEPlatform_ENUM_GENERATION_HEADERS}
    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WPEPlatform/wpe/WPEEnumTypes.h.in ${WPEPlatform_ENUM_GENERATION_HEADERS} | sed s/w_pe/wpe/ | sed s/WPE_TYPE_PE/WPE_TYPE/ | sed s/WPE_TYPEEGL/WPE_TYPE_EGL/ | sed s/wpeegl/wpe_egl/ > ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.h
    VERBATIM
)
add_custom_command(
    OUTPUT ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.cpp
    DEPENDS ${WPEPlatform_ENUM_GENERATION_HEADERS}
    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WPEPlatform/wpe/WPEEnumTypes.cpp.in ${WPEPlatform_ENUM_GENERATION_HEADERS} | sed s/w_pe/wpe/ | sed s/WPE_TYPE_PE/WPE_TYPE/ | sed s/WPE_TYPEEGL/WPE_TYPE_GL/ | sed s/wpeegl/wpe_egl/ > ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.cpp
    VERBATIM
)
add_custom_target(WPEPlatformGeneratedEnumTypesHeader
    DEPENDS ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe/WPEEnumTypes.h
)

set(WPEPlatform_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WPEPlatform_DERIVED_SOURCES_DIR}"
    "${WPEPlatform_DERIVED_SOURCES_DIR}/wpe"
    "${WEBKIT_DIR}/WPEPlatform"
    "${WEBKIT_DIR}/WPEPlatform/wpe"
    "${WTF_FRAMEWORK_HEADERS_DIR}"
)

set(WPEPlatform_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

set(WPEPlatform_LIBRARIES
    Epoxy::Epoxy
    WTF
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBXKBCOMMON_LIBRARIES}
)

if (USE_GBM)
    list(APPEND WPEPlatform_LIBRARIES GBM::GBM)
elseif (USE_LIBDRM)
    list(APPEND WPEPlatform_LIBRARIES LibDRM::LibDRM)
endif ()

if (NOT USE_SYSTEM_MALLOC)
   list(APPEND WPEPlatform_LIBRARIES bmalloc)
endif ()

if (ENABLE_WPE_PLATFORM_DRM)
    add_subdirectory(${WEBKIT_DIR}/WPEPlatform/wpe/drm)
    list(APPEND WPEPlatform_LIBRARIES WPEPlatformDRM)
endif ()

if (ENABLE_WPE_PLATFORM_HEADLESS)
    add_subdirectory(${WEBKIT_DIR}/WPEPlatform/wpe/headless)
    list(APPEND WPEPlatform_LIBRARIES WPEPlatformHeadless)
endif ()

if (ENABLE_WPE_PLATFORM_WAYLAND)
    add_subdirectory(${WEBKIT_DIR}/WPEPlatform/wpe/wayland)
    list(APPEND WPEPlatform_LIBRARIES WPEPlatformWayland)
endif ()

add_library(WPEPlatform-${WPE_API_VERSION} SHARED ${WPEPlatform_SOURCES})
set_target_properties(WPEPlatform-${WPE_API_VERSION} PROPERTIES VERSION ${WPE_PLATFORM_VERSION} SOVERSION ${WPE_PLATFORM_VERSION_MAJOR})
target_include_directories(WPEPlatform-${WPE_API_VERSION} PRIVATE ${WPEPlatform_PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(WPEPlatform-${WPE_API_VERSION} SYSTEM PRIVATE ${WPEPlatform_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEPlatform-${WPE_API_VERSION} ${WPEPlatform_LIBRARIES})

GI_INTROSPECT(WPEPlatform ${WPE_API_VERSION} wpe/wpe-platform.h
    TARGET WPEPlatform-${WPE_API_VERSION}
    PACKAGE wpe-platform
    IDENTIFIER_PREFIX WPE
    SYMBOL_PREFIX wpe
    DEPENDENCIES Gio-2.0
    OPTIONS
        -I${WEBKIT_DIR}/WPEPlatform
        -I${WPEPlatform_DERIVED_SOURCES_DIR}
    SOURCES
        ${WPEPlatform_INSTALLED_HEADERS}
        ${WPEPlatform_SOURCES}
    NO_IMPLICIT_SOURCES
)

GI_DOCGEN(WPEPlatform docs/wpeplatform.toml.in)

if (ENABLE_WPE_PLATFORM_DRM)
    get_target_property(WPEPlatformDRM_SOURCES_FOR_INTROSPECTION WPEPlatformDRM SOURCES_FOR_INTROSPECTION)
    GI_INTROSPECT(WPEPlatformDRM ${WPE_API_VERSION} wpe/drm/wpe-drm.h
        TARGET WPEPlatform-${WPE_API_VERSION}
        PACKAGE wpe-platform-drm
        IDENTIFIER_PREFIX WPE
        SYMBOL_PREFIX wpe
        DEPENDENCIES WPEPlatform
        OPTIONS
            -I${WEBKIT_DIR}/WPEPlatform
            -I${WPEPlatform_DERIVED_SOURCES_DIR}
        SOURCES
            ${WPEPlatformDRM_SOURCES_FOR_INTROSPECTION}
        NO_IMPLICIT_SOURCES
    )

    GI_DOCGEN(WPEPlatformDRM docs/wpeplatform-drm.toml.in)
endif ()

if (ENABLE_WPE_PLATFORM_HEADLESS)
    get_target_property(WPEPlatformHeadless_SOURCES_FOR_INTROSPECTION WPEPlatformHeadless SOURCES_FOR_INTROSPECTION)
    GI_INTROSPECT(WPEPlatformHeadless ${WPE_API_VERSION} wpe/headless/wpe-headless.h
        TARGET WPEPlatform-${WPE_API_VERSION}
        PACKAGE wpe-platform-headless
        IDENTIFIER_PREFIX WPE
        SYMBOL_PREFIX wpe
        DEPENDENCIES WPEPlatform
        OPTIONS
            -I${WEBKIT_DIR}/WPEPlatform
            -I${WPEPlatform_DERIVED_SOURCES_DIR}
        SOURCES
            ${WPEPlatformHeadless_SOURCES_FOR_INTROSPECTION}
        NO_IMPLICIT_SOURCES
    )

    GI_DOCGEN(WPEPlatformHeadless docs/wpeplatform-headless.toml.in)
endif ()

if (ENABLE_WPE_PLATFORM_WAYLAND)
    get_target_property(WPEPlatformWayland_SOURCES_FOR_INTROSPECTION WPEPlatformWayland SOURCES_FOR_INTROSPECTION)
    GI_INTROSPECT(WPEPlatformWayland ${WPE_API_VERSION} wpe/wayland/wpe-wayland.h
        TARGET WPEPlatform-${WPE_API_VERSION}
        PACKAGE wpe-platform-wayland
        IDENTIFIER_PREFIX WPE
        SYMBOL_PREFIX wpe
        DEPENDENCIES WPEPlatform
        OPTIONS
            -I${WEBKIT_DIR}/WPEPlatform
            -I${WPEPlatform_DERIVED_SOURCES_DIR}
        SOURCES
            ${WPEPlatformWayland_SOURCES_FOR_INTROSPECTION}
        NO_IMPLICIT_SOURCES
    )

    GI_DOCGEN(WPEPlatformWayland docs/wpeplatform-wayland.toml.in)
endif ()

install(TARGETS WPEPlatform-${WPE_API_VERSION} LIBRARY DESTINATION "${LIB_INSTALL_DIR}")

install(FILES "${CMAKE_BINARY_DIR}/wpe-platform-${WPE_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
        COMPONENT "Development"
)

install(FILES ${WPEPlatform_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/wpe-platform/wpe"
        COMPONENT "Development"
)
