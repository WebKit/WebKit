find_library(COCOA_LIBRARY Cocoa)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(READLINE_LIBRARY Readline)
list(APPEND JavaScriptCore_LIBRARIES
    ${COREFOUNDATION_LIBRARY}
    ${COCOA_LIBRARY}
    ${READLINE_LIBRARY}
    libicucore.dylib
)

list(APPEND JavaScriptCore_SOURCES
    API/JSAPIWrapperObject.mm
    API/JSContext.mm
    API/JSManagedValue.mm
    API/JSStringRefCF.cpp
    API/JSValue.mm
    API/JSVirtualMachine.mm
    API/JSWrapperMap.mm
    API/ObjCCallbackFunction.mm

    inspector/remote/RemoteInspector.mm
    inspector/remote/RemoteInspectorDebuggable.cpp
    inspector/remote/RemoteInspectorDebuggableConnection.mm
    inspector/remote/RemoteInspectorXPCConnection.mm
)
add_definitions(-DSTATICALLY_LINKED_WITH_WTF)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/TracingDtrace.h
    DEPENDS ${JAVASCRIPTCORE_DIR}/runtime/Tracing.d
    WORKING_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
    COMMAND dtrace -h -o "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/TracingDtrace.h" -s "${JAVASCRIPTCORE_DIR}/runtime/Tracing.d";
    VERBATIM)

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${JAVASCRIPTCORE_DIR}/disassembler/udis86
    ${JAVASCRIPTCORE_DIR}/icu
)
list(APPEND JavaScriptCore_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/TracingDtrace.h
)
