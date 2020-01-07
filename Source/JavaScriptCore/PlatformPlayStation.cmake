list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/inspector/remote/socket"
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

list(APPEND JavaScriptCore_SOURCES
    API/JSRemoteInspector.cpp

    inspector/remote/RemoteAutomationTarget.cpp
    inspector/remote/RemoteConnectionToTarget.cpp
    inspector/remote/RemoteControllableTarget.cpp
    inspector/remote/RemoteInspectionTarget.cpp
    inspector/remote/RemoteInspector.cpp

    inspector/remote/socket/RemoteInspectorConnectionClient.cpp
    inspector/remote/socket/RemoteInspectorMessageParser.cpp
    inspector/remote/socket/RemoteInspectorServer.cpp
    inspector/remote/socket/RemoteInspectorSocket.cpp
    inspector/remote/socket/RemoteInspectorSocketEndpoint.cpp

    inspector/remote/socket/posix/RemoteInspectorSocketPOSIX.cpp
)

# With the VisualStudio generator, the compiler complains about -std=c++* for C sources.
set_source_files_properties(
    disassembler/udis86/udis86.c
    disassembler/udis86/udis86_decode.c
    disassembler/udis86/udis86_itab_holder.c
    disassembler/udis86/udis86_syn-att.c
    disassembler/udis86/udis86_syn-intel.c
    disassembler/udis86/udis86_syn.c
    PROPERTIES LANGUAGE CXX
)

if (${WTF_LIBRARY_TYPE} STREQUAL "STATIC")
    add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
endif ()

# This overrides the default x64 value of 1GB for the memory pool size
add_definitions(-DFIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64)
