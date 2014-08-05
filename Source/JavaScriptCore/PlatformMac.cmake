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
