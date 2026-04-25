list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "inspector/remote/SourcesSocket.txt"
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/inspector/remote/socket"
)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSRemoteInspectorServer.h
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/RemoteAutomationTarget.h
    inspector/remote/RemoteConnectionToTarget.h
    inspector/remote/RemoteControllableTarget.h
    inspector/remote/RemoteInspectionTarget.h
    inspector/remote/RemoteInspector.h

    inspector/remote/socket/RemoteInspectorConnectionClient.h
    inspector/remote/socket/RemoteInspectorMessageParser.h
    inspector/remote/socket/RemoteInspectorServer.h
    inspector/remote/socket/RemoteInspectorSocket.h
    inspector/remote/socket/RemoteInspectorSocketEndpoint.h
)

if (UNIX)
    list(APPEND JavaScriptCore_SOURCES inspector/remote/socket/posix/RemoteInspectorSocketPOSIX.cpp)
else ()
    list(APPEND JavaScriptCore_SOURCES inspector/remote/socket/win/RemoteInspectorSocketWin.cpp)
    list(APPEND JavaScriptCore_LIBRARIES
        ws2_32
        wsock32
    )
endif ()
