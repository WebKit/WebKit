add_definitions(-DSTATICALLY_LINKED_WITH_WTF -D__STDC_WANT_LIB_EXT1__)

find_library(SECURITY_LIBRARY Security)
list(APPEND JavaScriptCore_LIBRARIES
    ${SECURITY_LIBRARY}
)

list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_WTF_DIR}
    ${JAVASCRIPTCORE_DIR}/disassembler/udis86
    ${JAVASCRIPTCORE_DIR}/inspector/cocoa
    ${JAVASCRIPTCORE_DIR}/inspector/remote/cocoa
)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSContext.h
    API/JSExport.h
    API/JSManagedValue.h
    API/JSStringRefCF.h
    API/JSValue.h
    API/JSVirtualMachine.h
    API/JavaScriptCore.h
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/cocoa/RemoteInspectorXPCConnection.h
)

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")

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
