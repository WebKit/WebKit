include(GLib.cmake)
include(inspector/remote/GLib.cmake)

list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
)

list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

if (USE_SHARED_JSC)
    set(JavaScriptCore_OUTPUT_NAME wpejavascriptcore-${WPE_API_VERSION})

    configure_file(wpejavascriptcore.pc.in ${JavaScriptCore_PKGCONFIG_FILE} @ONLY)

    if (EXISTS "${TOOLS_DIR}/glib/apply-build-revision-to-files.py")
        add_custom_target(JavaScriptCore-build-revision
            ${PYTHON_EXECUTABLE} "${TOOLS_DIR}/glib/apply-build-revision-to-files.py" ${JavaScriptCore_PKGCONFIG_FILE}
            DEPENDS ${JavaScriptCore_PKGCONFIG_FILE}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
        list(APPEND JavaScriptCore_DEPENDENCIES
            JavaScriptCore-build-revision
        )
    endif ()

    install(FILES "${CMAKE_BINARY_DIR}/Source/JavaScriptCore/wpejavascriptcore-${WPE_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
    )
endif ()

list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    PKGLIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}/wpe-webkit-${WPE_API_VERSION}"
)

list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    PKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/wpe-webkit-${WPE_API_VERSION}"
)

install(FILES ${JavaScriptCore_INSTALLED_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/jsc"
    COMPONENT "Development"
)

if (USE_SHARED_JSC)
    GI_INTROSPECT(JavaScriptCore ${WPE_API_VERSION} jsc/jsc.h
        PACKAGE wpejavascriptcore
        SYMBOL_PREFIX jsc
        DEPENDENCIES GObject-2.0
    )
    GI_DOCGEN(JavaScriptCore API/glib/docs/jsc.toml.in)
endif ()
