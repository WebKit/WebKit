set(TestNetscapePlugIn_DIR "${TOOLS_DIR}/DumpRenderTree/TestNetscapePlugIn")
set(TestNetscapePlugIn_LIBRARY_TYPE SHARED)

set(TestNetscapePlugIn_SOURCES
    PluginObject.cpp
    PluginTest.cpp
    TestObject.cpp
    main.cpp

    Tests/DocumentOpenInDestroyStream.cpp
    Tests/EvaluateJSAfterRemovingPluginElement.cpp
    Tests/FormValue.cpp
    Tests/GetURLNotifyWithURLThatFailsToLoad.cpp
    Tests/GetURLWithJavaScriptURL.cpp
    Tests/GetURLWithJavaScriptURLDestroyingPlugin.cpp
    Tests/GetUserAgentWithNullNPPFromNPPNew.cpp
    Tests/LogNPPSetWindow.cpp
    Tests/NPDeallocateCalledBeforeNPShutdown.cpp
    Tests/NPPNewFails.cpp
    Tests/NPPSetWindowCalledDuringDestruction.cpp
    Tests/NPRuntimeCallsWithNullNPP.cpp
    Tests/NPRuntimeObjectFromDestroyedPlugin.cpp
    Tests/NPRuntimeRemoveProperty.cpp
    Tests/NullNPPGetValuePointer.cpp
    Tests/PassDifferentNPPStruct.cpp
    Tests/PluginScriptableNPObjectInvokeDefault.cpp
    Tests/PluginScriptableObjectOverridesAllProperties.cpp
    Tests/PrivateBrowsing.cpp
    Tests/ToStringAndValueOfObject.cpp
    Tests/URLRedirect.cpp
)

set(TestNetscapePlugIn_PRIVATE_INCLUDE_DIRECTORIES
    ${TestNetscapePlugIn_DIR}
    ${TestNetscapePlugIn_DIR}/ForwardingHeaders
    ${TestNetscapePlugIn_DIR}/Tests
)

set(TestNetscapePlugIn_LIBRARIES
    WebKit::WTF
)

# The ForwardingHeaders directory contains np*.h which are copied from WebCore
# so depend on the WebKit::WebCore target when building
set(TestNetscapePlugIn_DEPENDENCIES WebKit::WebCore)
list(APPEND TestNetscapePlugIn_PRIVATE_INCLUDE_DIRECTORIES ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR})

WEBKIT_FRAMEWORK_DECLARE(TestNetscapePlugIn)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_FRAMEWORK(TestNetscapePlugIn)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestNetscapePlugIn -Wno-unused-parameter)
endif ()
