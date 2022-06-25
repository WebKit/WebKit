include(GLib.cmake)
include(inspector/remote/GLib.cmake)
set(JavaScriptCore_OUTPUT_NAME javascriptcoregtk-${WEBKITGTK_API_VERSION})

configure_file(javascriptcoregtk.pc.in ${JavaScriptCore_PKGCONFIG_FILE} @ONLY)

if (EXISTS "${TOOLS_DIR}/glib/apply-build-revision-to-files.py")
    add_custom_target(JavaScriptCore-build-revision
        ${PYTHON_EXECUTABLE} "${TOOLS_DIR}/glib/apply-build-revision-to-files.py" ${JavaScriptCore_PKGCONFIG_FILE}
        DEPENDS ${JavaScriptCore_PKGCONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
    list(APPEND JavaScriptCore_DEPENDENCIES
        JavaScriptCore-build-revision
    )
endif ()

install(FILES "${CMAKE_BINARY_DIR}/Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)

install(FILES ${JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/JavaScriptCore"
)

install(FILES ${JavaScriptCore_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/jsc"
)

list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS JSC_COMPILATION)

list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
)
list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

GI_INTROSPECT(JavaScriptCore ${WEBKITGTK_API_VERSION} jsc/jsc.h
    PACKAGE javascriptcoregtk
    SYMBOL_PREFIX jsc
    DEPENDENCIES GObject-2.0
)
GI_DOCGEN(JavaScriptCore API/glib/docs/jsc.toml.in)
