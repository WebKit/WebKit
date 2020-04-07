set(DumpRenderTree_DIR "${TOOLS_DIR}/DumpRenderTree")
set(DumpRenderTree_BINDINGS_DIR "${TOOLS_DIR}/DumpRenderTree/Bindings")
set(WebKitTestRunner_SHARED_DIR "${TOOLS_DIR}/TestRunnerShared/")
set(WebKitTestRunner_UISCRIPTCONTEXT_DIR "${TOOLS_DIR}/TestRunnerShared/UIScriptContext")

file(MAKE_DIRECTORY ${DumpRenderTree_DERIVED_SOURCES_DIR})

set(DumpRenderTree_SOURCES
    ${WebKitTestRunner_SHARED_DIR}/Bindings/JSWrapper.cpp

    ${WebKitTestRunner_UISCRIPTCONTEXT_DIR}/UIScriptContext.cpp
    ${WebKitTestRunner_UISCRIPTCONTEXT_DIR}/UIScriptController.cpp

    AccessibilityController.cpp
    AccessibilityTextMarker.cpp
    AccessibilityUIElement.cpp
    CyclicRedundancyCheck.cpp
    DumpRenderTreeCommon.cpp
    GCController.cpp
    JavaScriptThreading.cpp
    PixelDumpSupport.cpp
    TestOptions.cpp
    TestRunner.cpp
    WorkQueue.cpp
)

set(DumpRenderTree_LIBRARIES
    WebCoreTestSupport
    WebKit::JavaScriptCore
)

set(DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${DumpRenderTree_DERIVED_SOURCES_DIR}
    ${DumpRenderTree_DIR}
    ${WebKitTestRunner_SHARED_DIR}/Bindings
    ${WebKitTestRunner_UISCRIPTCONTEXT_DIR}
)

list(APPEND DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${WEBCORE_DIR}/testing/js
)

list(APPEND DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
)

set(TestNetscapePlugIn_SOURCES
    TestNetscapePlugIn/PluginObject.cpp
    TestNetscapePlugIn/PluginTest.cpp
    TestNetscapePlugIn/TestObject.cpp
    TestNetscapePlugIn/main.cpp

    TestNetscapePlugIn/Tests/DocumentOpenInDestroyStream.cpp
    TestNetscapePlugIn/Tests/EvaluateJSAfterRemovingPluginElement.cpp
    TestNetscapePlugIn/Tests/FormValue.cpp
    TestNetscapePlugIn/Tests/GetURLNotifyWithURLThatFailsToLoad.cpp
    TestNetscapePlugIn/Tests/GetURLWithJavaScriptURL.cpp
    TestNetscapePlugIn/Tests/GetURLWithJavaScriptURLDestroyingPlugin.cpp
    TestNetscapePlugIn/Tests/GetUserAgentWithNullNPPFromNPPNew.cpp
    TestNetscapePlugIn/Tests/LogNPPSetWindow.cpp
    TestNetscapePlugIn/Tests/NPDeallocateCalledBeforeNPShutdown.cpp
    TestNetscapePlugIn/Tests/NPPNewFails.cpp
    TestNetscapePlugIn/Tests/NPPSetWindowCalledDuringDestruction.cpp
    TestNetscapePlugIn/Tests/NPRuntimeCallsWithNullNPP.cpp
    TestNetscapePlugIn/Tests/NPRuntimeObjectFromDestroyedPlugin.cpp
    TestNetscapePlugIn/Tests/NPRuntimeRemoveProperty.cpp
    TestNetscapePlugIn/Tests/NullNPPGetValuePointer.cpp
    TestNetscapePlugIn/Tests/PassDifferentNPPStruct.cpp
    TestNetscapePlugIn/Tests/PluginScriptableNPObjectInvokeDefault.cpp
    TestNetscapePlugIn/Tests/PluginScriptableObjectOverridesAllProperties.cpp
    TestNetscapePlugIn/Tests/PrivateBrowsing.cpp
    TestNetscapePlugIn/Tests/ToStringAndValueOfObject.cpp
    TestNetscapePlugIn/Tests/URLRedirect.cpp
)

set(TestNetscapePlugIn_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${DumpRenderTree_DIR}/TestNetscapePlugIn
    ${DumpRenderTree_DIR}/TestNetscapePlugIn/ForwardingHeaders
    ${DumpRenderTree_DIR}/TestNetscapePlugIn/Tests
)

list(APPEND TestNetscapePlugIn_PRIVATE_INCLUDE_DIRECTORIES
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
)

set(TestNetscapePlugIn_LIBRARIES
    WebCoreTestSupport
    WebKit::JavaScriptCore
)

set(DumpRenderTree_IDL_FILES
    "${WebKitTestRunner_UISCRIPTCONTEXT_DIR}/Bindings/UIScriptController.idl"
)

GENERATE_BINDINGS(DumpRenderTreeBindings
    OUTPUT_SOURCE DumpRenderTree_SOURCES
    INPUT_FILES ${DumpRenderTree_IDL_FILES}
    BASE_DIR ${DumpRenderTree_BINDINGS_DIR}
    IDL_INCLUDES Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${DumpRenderTree_DERIVED_SOURCES_DIR}
    GENERATOR DumpRenderTree)

set(DumpRenderTree_DEPENDENCIES DumpRenderTreeBindings)

WEBKIT_EXECUTABLE_DECLARE(DumpRenderTree)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_EXECUTABLE(DumpRenderTree)

if (ENABLE_NETSCAPE_PLUGIN_API)
    add_library(TestNetscapePlugIn SHARED ${TestNetscapePlugIn_SOURCES})
    target_include_directories(TestNetscapePlugIn PRIVATE ${TestNetscapePlugIn_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(TestNetscapePlugIn PRIVATE ${TestNetscapePlugIn_LIBRARIES})
endif ()
