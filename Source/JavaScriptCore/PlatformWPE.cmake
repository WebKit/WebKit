include(GLib.cmake)
include(inspector/remote/GLib.cmake)

list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
)

list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    JSC_COMPILATION
    PKGLIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}/wpe-webkit-${WPE_API_VERSION}"
)

install(FILES ${JavaScriptCore_INSTALLED_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/jsc"
    COMPONENT "Development"
)
