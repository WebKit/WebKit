list(APPEND JavaScriptCore_LIBRARIES
    ${GLIB_LIBRARIES}
)

list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${WTF_DIR}
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

set(WPE_INSTALLED_JAVASCRIPTCORE_HEADERS
    API/JSBase.h
    API/JSContextRef.h
    API/JSObjectRef.h
    API/JSStringRef.h
    API/JSTypedArray.h
    API/JSValueRef.h
    API/JavaScript.h
    API/WebKitAvailability.h
)

install(FILES ${WPE_INSTALLED_JAVASCRIPTCORE_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-${WPE_API_VERSION}/WPE/JavaScriptCore"
    COMPONENT "Development"
)
