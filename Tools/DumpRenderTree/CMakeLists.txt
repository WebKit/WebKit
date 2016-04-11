set(DumpRenderTree_SOURCES
    AccessibilityController.cpp
    AccessibilityTextMarker.cpp
    AccessibilityUIElement.cpp
    CyclicRedundancyCheck.cpp
    DumpRenderTreeCommon.cpp
    GCController.cpp
    JavaScriptThreading.cpp
    PixelDumpSupport.cpp
    TestRunner.cpp
    WorkQueue.cpp
)

set(DumpRenderTree_LIBRARIES
    JavaScriptCore
    WTF
    WebCoreTestSupport
)

set(DumpRenderTree_INCLUDE_DIRECTORIES
    ${WEBCORE_DIR}
    ${WEBCORE_DIR}/bindings
    ${WEBCORE_DIR}/bridge
    ${WEBCORE_DIR}/bridge/jsc
    ${WEBCORE_DIR}/css
    ${WEBCORE_DIR}/dom
    ${WEBCORE_DIR}/editing
    ${WEBCORE_DIR}/history
    ${WEBCORE_DIR}/html
    ${WEBCORE_DIR}/inspector
    ${WEBCORE_DIR}/loader
    ${WEBCORE_DIR}/loader/cache
    ${WEBCORE_DIR}/loader/icon
    ${WEBCORE_DIR}/page
    ${WEBCORE_DIR}/page/animation
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/animation
    ${WEBCORE_DIR}/platform/graphics
    ${WEBCORE_DIR}/platform/graphics/transforms
    ${WEBCORE_DIR}/platform/network
    ${WEBCORE_DIR}/platform/text
    ${WEBCORE_DIR}/plugins
    ${WEBCORE_DIR}/rendering
    ${WEBCORE_DIR}/rendering/shapes
    ${WEBCORE_DIR}/rendering/style
    ${JAVASCRIPTCORE_DIR}
    ${JAVASCRIPTCORE_DIR}/API
    ${JAVASCRIPTCORE_DIR}/assembler
    ${JAVASCRIPTCORE_DIR}/bytecode
    ${JAVASCRIPTCORE_DIR}/dfg
    ${JAVASCRIPTCORE_DIR}/disassembler
    ${JAVASCRIPTCORE_DIR}/heap
    ${JAVASCRIPTCORE_DIR}/interpreter
    ${JAVASCRIPTCORE_DIR}/jit
    ${JAVASCRIPTCORE_DIR}/llint
    ${JAVASCRIPTCORE_DIR}/parser
    ${JAVASCRIPTCORE_DIR}/profiler
    ${JAVASCRIPTCORE_DIR}/runtime
    ${JAVASCRIPTCORE_DIR}/ForwardingHeaders
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
    ${TOOLS_DIR}/DumpRenderTree
    ${WTF_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_BINARY_DIR}
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_WEBCORE_DIR}
    ${WEBCORE_DIR}/bindings/js
    ${WEBCORE_DIR}/testing/js
)

set(TestNetscapePlugin_SOURCES
    TestNetscapePlugin/PluginObject.cpp
    TestNetscapePlugin/PluginTest.cpp
    TestNetscapePlugin/TestObject.cpp
    TestNetscapePlugin/main.cpp

    TestNetscapePlugin/Tests/DocumentOpenInDestroyStream.cpp
    TestNetscapePlugin/Tests/EvaluateJSAfterRemovingPluginElement.cpp
    TestNetscapePlugin/Tests/FormValue.cpp
    TestNetscapePlugin/Tests/GetURLNotifyWithURLThatFailsToLoad.cpp
    TestNetscapePlugin/Tests/GetURLWithJavaScriptURL.cpp
    TestNetscapePlugin/Tests/GetURLWithJavaScriptURLDestroyingPlugin.cpp
    TestNetscapePlugin/Tests/GetUserAgentWithNullNPPFromNPPNew.cpp
    TestNetscapePlugin/Tests/LogNPPSetWindow.cpp
    TestNetscapePlugin/Tests/NPDeallocateCalledBeforeNPShutdown.cpp
    TestNetscapePlugin/Tests/NPPNewFails.cpp
    TestNetscapePlugin/Tests/NPPSetWindowCalledDuringDestruction.cpp
    TestNetscapePlugin/Tests/NPRuntimeCallsWithNullNPP.cpp
    TestNetscapePlugin/Tests/NPRuntimeObjectFromDestroyedPlugin.cpp
    TestNetscapePlugin/Tests/NPRuntimeRemoveProperty.cpp
    TestNetscapePlugin/Tests/NullNPPGetValuePointer.cpp
    TestNetscapePlugin/Tests/PassDifferentNPPStruct.cpp
    TestNetscapePlugin/Tests/PluginScriptableNPObjectInvokeDefault.cpp
    TestNetscapePlugin/Tests/PluginScriptableObjectOverridesAllProperties.cpp
    TestNetscapePlugin/Tests/PrivateBrowsing.cpp
    TestNetscapePlugin/Tests/ToStringAndValueOfObject.cpp
    TestNetscapePlugin/Tests/URLRedirect.cpp
)

set(TestNetscapePlugin_LIBRARIES
    JavaScriptCore
    WTF
    WebCoreTestSupport
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(${DumpRenderTree_INCLUDE_DIRECTORIES})

add_executable(DumpRenderTree ${DumpRenderTree_SOURCES})
target_link_libraries(DumpRenderTree ${DumpRenderTree_LIBRARIES})

add_library(TestNetscapePlugin SHARED ${TestNetscapePlugin_SOURCES})
target_link_libraries(TestNetscapePlugin ${TestNetscapePlugin_LIBRARIES})

if (WIN32)
    add_dependencies(DumpRenderTree DumpRenderTreeLib)
endif ()
