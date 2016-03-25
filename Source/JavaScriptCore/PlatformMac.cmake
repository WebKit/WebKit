list(APPEND JavaScriptCore_SOURCES
    API/JSAPIWrapperObject.mm
    API/JSContext.mm
    API/JSManagedValue.mm
    API/JSRemoteInspector.cpp
    API/JSStringRefCF.cpp
    API/JSValue.mm
    API/JSVirtualMachine.mm
    API/JSWrapperMap.mm
    API/ObjCCallbackFunction.mm

    inspector/remote/RemoteAutomationTarget.cpp
    inspector/remote/RemoteConnectionToTarget.mm
    inspector/remote/RemoteControllableTarget.cpp
    inspector/remote/RemoteInspectionTarget.cpp
    inspector/remote/RemoteInspector.mm
    inspector/remote/RemoteInspectorXPCConnection.mm
)
add_definitions(-DSTATICALLY_LINKED_WITH_WTF)

find_library(SECURITY_LIBRARY Security)
list(APPEND JavaScriptCore_LIBRARIES
    ${SECURITY_LIBRARY}
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/TracingDtrace.h
    DEPENDS ${JAVASCRIPTCORE_DIR}/runtime/Tracing.d
    WORKING_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
    COMMAND dtrace -h -o "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/TracingDtrace.h" -s "${JAVASCRIPTCORE_DIR}/runtime/Tracing.d"
    VERBATIM)

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${WTF_DIR}
    ${JAVASCRIPTCORE_DIR}/disassembler/udis86
    ${JAVASCRIPTCORE_DIR}/icu
)
list(APPEND JavaScriptCore_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/TracingDtrace.h
)

# FIXME: Make including these files consistent in the source so these forwarding headers are not needed.
if (NOT EXISTS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorBackendDispatchers.h)
    file(WRITE ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorBackendDispatchers.h "#include \"inspector/InspectorBackendDispatchers.h\"")
endif ()
if (NOT EXISTS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorFrontendDispatchers.h)
    file(WRITE ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorFrontendDispatchers.h "#include \"inspector/InspectorFrontendDispatchers.h\"")
endif ()
if (NOT EXISTS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorProtocolObjects.h)
    file(WRITE ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InspectorProtocolObjects.h "#include \"inspector/InspectorProtocolObjects.h\"")
endif ()
