include(GLib.cmake)

list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
)

list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

list(APPEND JavaScriptCore_SOURCES
    API/JSRemoteInspector.cpp

    inspector/remote/RemoteAutomationTarget.cpp
    inspector/remote/RemoteControllableTarget.cpp
    inspector/remote/RemoteInspectionTarget.cpp
    inspector/remote/RemoteInspector.cpp

    inspector/remote/glib/RemoteConnectionToTargetGlib.cpp
    inspector/remote/glib/RemoteInspectorGlib.cpp
    inspector/remote/glib/RemoteInspectorServer.cpp
    inspector/remote/glib/RemoteInspectorUtils.cpp
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/glib/RemoteInspectorServer.h
    inspector/remote/glib/RemoteInspectorUtils.h
)

install(FILES ${JavaScriptCore_INSTALLED_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/jsc"
    COMPONENT "Development"
)

add_definitions(-DJSC_COMPILATION)
add_definitions(-DPKGLIBDIR="${CMAKE_INSTALL_FULL_LIBDIR}/wpe-webkit-${WPE_API_VERSION}")
