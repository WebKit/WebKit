set(WEBKIT_TESTRUNNER_SHARED_DIR "${TOOLS_DIR}/TestRunnerShared/")
set(WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR "${TOOLS_DIR}/TestRunnerShared/UIScriptContext")
set(DUMP_RENDER_TREE_BINDINGS_DIR "${TOOLS_DIR}/DumpRenderTree/Bindings")

file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/DumpRenderTree)

set(DumpRenderTree_SOURCES
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
    ${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}/UIScriptContext.cpp
    ${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}/UIScriptController.cpp
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/Bindings/JSWrapper.cpp
)

set(DumpRenderTree_LIBRARIES
    WebCoreTestSupport
    WebKit::JavaScriptCore
)

set(DumpRenderTree_INCLUDE_DIRECTORIES
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${DERIVED_SOURCES_DIR}/DumpRenderTree
    ${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/Bindings
    ${TOOLS_DIR}/DumpRenderTree
    ${CMAKE_SOURCE_DIR}/Source
    ${WEBCORE_DIR}/testing/js
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

set(TestNetscapePlugIn_LIBRARIES
    WebKit::JavaScriptCore
    WebCoreTestSupport
)

set(DumpRenderTree_IDL_FILES
    "${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}/Bindings/UIScriptController.idl"
)

GENERATE_BINDINGS(DumpRenderTreeBindings
    OUTPUT_SOURCE DumpRenderTree_SOURCES
    INPUT_FILES ${DumpRenderTree_IDL_FILES}
    BASE_DIR ${DUMP_RENDER_TREE_BINDINGS_DIR}
    IDL_INCLUDES Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${DERIVED_SOURCES_DIR}/DumpRenderTree
    GENERATOR DumpRenderTree)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(${DumpRenderTree_INCLUDE_DIRECTORIES})

add_executable(DumpRenderTree ${DumpRenderTree_SOURCES})
target_link_libraries(DumpRenderTree ${DumpRenderTree_LIBRARIES})
add_dependencies(DumpRenderTree DumpRenderTreeBindings)

if (ENABLE_NETSCAPE_PLUGIN_API)
    add_library(TestNetscapePlugIn SHARED ${TestNetscapePlugIn_SOURCES})
    target_link_libraries(TestNetscapePlugIn ${TestNetscapePlugIn_LIBRARIES})
endif ()

if (WIN32)
    add_dependencies(DumpRenderTree DumpRenderTreeLib)
endif ()
