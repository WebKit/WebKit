set(WEBKIT_TESTNETSCAPEPLUGIN_DIR "${TOOLS_DIR}/DumpRenderTree/TestNetscapePlugIn")

set(WebKitTestNetscapePlugin_SOURCES
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/PluginObject.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/PluginTest.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/TestObject.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/main.cpp

    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/DocumentOpenInDestroyStream.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/EvaluateJSAfterRemovingPluginElement.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/FormValue.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/GetURLNotifyWithURLThatFailsToLoad.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/GetURLWithJavaScriptURL.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/GetURLWithJavaScriptURLDestroyingPlugin.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/GetUserAgentWithNullNPPFromNPPNew.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/LogNPPSetWindow.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/NPDeallocateCalledBeforeNPShutdown.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/NPPNewFails.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/NPPSetWindowCalledDuringDestruction.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/NPRuntimeCallsWithNullNPP.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/NPRuntimeObjectFromDestroyedPlugin.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/NPRuntimeRemoveProperty.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/NullNPPGetValuePointer.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/PassDifferentNPPStruct.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/PluginScriptableNPObjectInvokeDefault.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/PluginScriptableObjectOverridesAllProperties.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/PrivateBrowsing.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/ToStringAndValueOfObject.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/URLRedirect.cpp
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/x11/CallInvalidateRectWithNullNPPArgument.cpp
)

set(WebKitTestNetscapePlugin_INCLUDE_DIRECTORIES
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/ForwardingHeaders
    ${WEBCORE_DIR}
    ${WTF_DIR}
)

set(WebKitTestNetscapePlugin_SYSTEM_INCLUDE_DIRECTORIES
    ${X11_INCLUDE_DIR}
)

include_directories(${WebKitTestNetscapePlugin_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${WebKitTestNetscapePlugin_SYSTEM_INCLUDE_DIRECTORIES})

set(WebKitTestNetscapePlugin_LIBRARIES
    ${X11_LIBRARIES}
)

if (WTF_OS_UNIX)
    add_definitions(-DXP_UNIX)
endif ()

add_library(TestNetscapePlugin SHARED ${WebKitTestNetscapePlugin_SOURCES})
target_link_libraries(TestNetscapePlugin ${WebKitTestNetscapePlugin_LIBRARIES})
set_target_properties(TestNetscapePlugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins)
WEBKIT_SET_EXTRA_COMPILER_FLAGS(TestNetscapePlugin)

# Suppress unused parameter warnings for sources in WebKit2.
ADD_TARGET_PROPERTIES(TestNetscapePlugin COMPILE_FLAGS "-Wno-unused-parameter")
