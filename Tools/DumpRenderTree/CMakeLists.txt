set(DumpRenderTree_DIR "${TOOLS_DIR}/DumpRenderTree")
set(DumpRenderTree_BINDINGS_DIR "${TOOLS_DIR}/DumpRenderTree/Bindings")
set(WebKitTestRunner_SHARED_DIR "${TOOLS_DIR}/TestRunnerShared/")
set(WebKitTestRunner_UISCRIPTCONTEXT_DIR "${TOOLS_DIR}/TestRunnerShared/UIScriptContext")

file(MAKE_DIRECTORY ${DumpRenderTree_DERIVED_SOURCES_DIR})

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
)

set(DumpRenderTree_LIBRARIES
    TestRunnerShared
    WebKit::WebCoreTestSupport
    WebKit::JavaScriptCore
)

set(DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${DumpRenderTree_DERIVED_SOURCES_DIR}
    ${DumpRenderTree_DIR}
    ${WebKitTestRunner_SHARED_DIR}
    ${WebKitTestRunner_SHARED_DIR}/Bindings
    ${WebKitTestRunner_UISCRIPTCONTEXT_DIR}
)

list(APPEND DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
)

WEBKIT_EXECUTABLE_DECLARE(DumpRenderTree)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_EXECUTABLE(DumpRenderTree)

if (ENABLE_NETSCAPE_PLUGIN_API)
    add_subdirectory(TestNetscapePlugIn)
endif ()
