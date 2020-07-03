add_definitions(-DSTATICALLY_LINKED_WITH_WTF -D__STDC_WANT_LIB_EXT1__)

find_library(SECURITY_LIBRARY Security)
list(APPEND JavaScriptCore_LIBRARIES
    ${SECURITY_LIBRARY}
)

list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_DIR}
    ${JAVASCRIPTCORE_DIR}/disassembler/udis86
    ${JAVASCRIPTCORE_DIR}/inspector/cocoa
    ${JAVASCRIPTCORE_DIR}/inspector/remote/cocoa
)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSCallbackFunction.h
    API/JSContext.h
    API/JSContextPrivate.h
    API/JSContextRefPrivate.h
    API/JSExport.h
    API/JSManagedValue.h
    API/JSStringRefCF.h
    API/JSValue.h
    API/JSValuePrivate.h
    API/JSVirtualMachine.h
    API/JavaScriptCore.h
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/cocoa/RemoteInspectorXPCConnection.h
)

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION} -force_load ${CMAKE_BINARY_DIR}/lib/libWTF.a")

# FIXME: Make including these files consistent in the source so these forwarding headers are not needed.
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/AugmentableInspectorControllerClient.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/AugmentableInspectorControllerClient.h "#include \"inspector/augmentable/AugmentableInspectorControllerClient.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendRouter.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendRouter.h "#include \"inspector/InspectorFrontendRouter.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatcher.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatcher.h "#include \"inspector/InspectorBackendDispatcher.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatchers.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatchers.h "#include \"inspector/InspectorBackendDispatchers.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendDispatchers.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendDispatchers.h "#include \"inspector/InspectorFrontendDispatchers.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorProtocolObjects.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorProtocolObjects.h "#include \"inspector/InspectorProtocolObjects.h\"")
endif ()
