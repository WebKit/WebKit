list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/inspector/remote/playstation"
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/RemoteAutomationTarget.h
    inspector/remote/RemoteConnectionToTarget.h
    inspector/remote/RemoteControllableTarget.h
    inspector/remote/RemoteInspectionTarget.h
    inspector/remote/RemoteInspector.h

    inspector/remote/playstation/RemoteInspectorConnectionClient.h
    inspector/remote/playstation/RemoteInspectorMessageParser.h
    inspector/remote/playstation/RemoteInspectorServer.h
    inspector/remote/playstation/RemoteInspectorSocket.h
    inspector/remote/playstation/RemoteInspectorSocketClient.h
    inspector/remote/playstation/RemoteInspectorSocketServer.h
)

list(APPEND JavaScriptCore_SOURCES
    API/JSRemoteInspector.cpp

    inspector/remote/RemoteAutomationTarget.cpp
    inspector/remote/RemoteConnectionToTarget.cpp
    inspector/remote/RemoteControllableTarget.cpp
    inspector/remote/RemoteInspectionTarget.cpp
    inspector/remote/RemoteInspector.cpp

    inspector/remote/playstation/RemoteInspectorConnectionClientPlayStation.cpp
    inspector/remote/playstation/RemoteInspectorMessageParserPlayStation.cpp
    inspector/remote/playstation/RemoteInspectorPlayStation.cpp
    inspector/remote/playstation/RemoteInspectorServerPlayStation.cpp
    inspector/remote/playstation/RemoteInspectorSocketClientPlayStation.cpp
    inspector/remote/playstation/RemoteInspectorSocketPlayStation.cpp
    inspector/remote/playstation/RemoteInspectorSocketServerPlayStation.cpp
)

if (${WTF_LIBRARY_TYPE} STREQUAL "STATIC")
    add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
endif ()

# This overrides the default x64 value of 1GB for the memory pool size
add_definitions(-DFIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64)
