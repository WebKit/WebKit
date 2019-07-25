set(WEBKIT_TESTNETSCAPEPLUGIN_DIR "${TOOLS_DIR}/DumpRenderTree/TestNetscapePlugIn")

set(WebKitTestNetscapePlugIn_SOURCES
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
)

set(WebKitTestNetscapePlugIn_INCLUDE_DIRECTORIES
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}
    ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/ForwardingHeaders
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WTF_DIR}
)

set(WebKitTestNetscapePlugIn_LIBRARIES
    WebKit::WTF
)

if (MOZ_X11)
    set(WebKitTestNetscapePlugIn_SYSTEM_INCLUDE_DIRECTORIES
        ${X11_INCLUDE_DIR}
    )

    list(APPEND WebKitTestNetscapePlugIn_LIBRARIES
        ${X11_LIBRARIES}
    )

    add_definitions(-DMOZ_X11)
endif ()

if (XP_UNIX)
    list(APPEND WebKitTestNetscapePlugIn_SOURCES
        ${WEBKIT_TESTNETSCAPEPLUGIN_DIR}/Tests/unix/CallInvalidateRectWithNullNPPArgument.cpp
    )

    add_definitions(-DXP_UNIX)
endif ()

include_directories(${WebKitTestNetscapePlugIn_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${WebKitTestNetscapePlugIn_SYSTEM_INCLUDE_DIRECTORIES})

add_library(TestNetscapePlugIn SHARED ${WebKitTestNetscapePlugIn_SOURCES})
target_link_libraries(TestNetscapePlugIn ${WebKitTestNetscapePlugIn_LIBRARIES})
add_dependencies(TestNetscapePlugIn WebCorePrivateFrameworkHeaders)
set_target_properties(TestNetscapePlugIn PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins)

WEBKIT_ADD_TARGET_CXX_FLAGS(TestNetscapePlugIn -Wno-unused-parameter)
