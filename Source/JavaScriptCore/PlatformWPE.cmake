include(GLib.cmake)
include(inspector/remote/GLib.cmake)

list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    PKGLIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}/wpe-webkit-${WPE_API_VERSION}"
)

install(FILES ${JavaScriptCore_INSTALLED_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/jsc"
    COMPONENT "Development"
)
