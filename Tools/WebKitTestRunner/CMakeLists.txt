set(WebKitTestRunner_DIR "${TOOLS_DIR}/WebKitTestRunner")
set(WebKitTestRunner_SHARED_DIR "${TOOLS_DIR}/TestRunnerShared/")
set(WebKitTestRunner_UISCRIPTCONTEXT_DIR "${WebKitTestRunner_SHARED_DIR}/UIScriptContext")
set(WebKitTestRunner_BINDINGS_DIR "${WebKitTestRunner_SHARED_DIR}/Bindings")

file(MAKE_DIRECTORY ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle)
file(MAKE_DIRECTORY ${WebKitTestRunner_DERIVED_SOURCES_DIR}/UIScriptContext)

set(WebKitTestRunner_SOURCES
    ${WebKitTestRunner_BINDINGS_DIR}/JSBasics.cpp
    ${WebKitTestRunner_BINDINGS_DIR}/JSWrapper.cpp

    ${WebKitTestRunner_UISCRIPTCONTEXT_DIR}/UIScriptContext.cpp
    ${WebKitTestRunner_UISCRIPTCONTEXT_DIR}/UIScriptControllerShared.cpp

    CyclicRedundancyCheck.cpp
    GeolocationProviderMock.cpp
    Options.cpp
    PixelDumpSupport.cpp
    TestController.cpp
    TestInvocation.cpp
    TestOptions.cpp
    UIScriptController.cpp
    WebNotificationProvider.cpp
    WorkQueueManager.cpp
)

set(WebKitTestRunner_LIBRARIES
    WebCoreTestSupport
    WebKit::JavaScriptCore
    WebKit::WebKit
)

set(WebKitTestRunner_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${WebKitTestRunner_BINDINGS_DIR}
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/UIScriptContext
    ${WebKitTestRunner_DIR}
    ${WebKitTestRunner_UISCRIPTCONTEXT_DIR}
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WEBCORE_DIR}/testing/js
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
)

set(WebKitTestRunnerInjectedBundle_SOURCES
    ${WebKitTestRunner_SHARED_DIR}/ReftestFunctions.cpp

    ${WebKitTestRunner_BINDINGS_DIR}/JSBasics.cpp
    ${WebKitTestRunner_BINDINGS_DIR}/JSWrapper.cpp

    InjectedBundle/AccessibilityController.cpp
    InjectedBundle/AccessibilityTextMarker.cpp
    InjectedBundle/AccessibilityTextMarkerRange.cpp
    InjectedBundle/AccessibilityUIElement.cpp
    InjectedBundle/EventSendingController.cpp
    InjectedBundle/GCController.cpp
    InjectedBundle/InjectedBundle.cpp
    InjectedBundle/InjectedBundleMain.cpp
    InjectedBundle/InjectedBundlePage.cpp
    InjectedBundle/TestRunner.cpp
    InjectedBundle/TextInputController.cpp
)

set(WebKitTestRunnerInjectedBundle_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${WebKitTestRunner_SHARED_DIR}
    ${WebKitTestRunner_BINDINGS_DIR}
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle
    ${WebKitTestRunner_DIR}
    ${WebKitTestRunner_DIR}/InjectedBundle
)

list(APPEND WebKitTestRunnerInjectedBundle_INCLUDE_DIRECTORIES
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
)

set(WebKitTestRunnerInjectedBundle_LIBRARIES
    WebKit::WebCoreTestSupport
    WebKit::WebKit
)

set(WebKitTestRunnerInjectedBundle_IDL_FILES
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityController.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityTextMarker.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityTextMarkerRange.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityUIElement.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/EventSendingController.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/GCController.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/TestRunner.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/TextInputController.idl"
)

set(WebKitTestRunner_IDL_FILES
    "${WebKitTestRunner_UISCRIPTCONTEXT_DIR}/Bindings/UIScriptController.idl"
)

set(WebKitTestRunner_DEPENDENCIES
    TestRunnerInjectedBundle
    WebKitTestRunnerBindings
)

GENERATE_BINDINGS(WebKitTestRunnerInjectedBundleBindings
    OUTPUT_SOURCE WebKitTestRunnerInjectedBundle_SOURCES
    INPUT_FILES ${WebKitTestRunnerInjectedBundle_IDL_FILES}
    BASE_DIR ${WebKitTestRunner_DIR}/InjectedBundle/Bindings
    IDL_INCLUDES ${WebKitTestRunner_DIR}/InjectedBundle/Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle
    GENERATOR TestRunner)

GENERATE_BINDINGS(WebKitTestRunnerBindings
    OUTPUT_SOURCE WebKitTestRunner_SOURCES
    INPUT_FILES ${WebKitTestRunner_IDL_FILES}
    BASE_DIR ${WebKitTestRunner_DIR}/InjectedBundle/Bindings
    IDL_INCLUDES ${WebKitTestRunner_DIR}/InjectedBundle/Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${WebKitTestRunner_DERIVED_SOURCES_DIR}/UIScriptContext
    GENERATOR TestRunner)

WEBKIT_EXECUTABLE_DECLARE(WebKitTestRunner)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

add_library(TestRunnerInjectedBundle SHARED ${WebKitTestRunnerInjectedBundle_SOURCES})
target_include_directories(TestRunnerInjectedBundle PRIVATE ${WebKitTestRunnerInjectedBundle_INCLUDE_DIRECTORIES})
target_link_libraries(TestRunnerInjectedBundle PRIVATE ${WebKitTestRunnerInjectedBundle_LIBRARIES})
add_dependencies(TestRunnerInjectedBundle WebKitTestRunnerInjectedBundleBindings)

WEBKIT_EXECUTABLE(WebKitTestRunner)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestRunnerInjectedBundle -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitTestRunner -Wno-unused-parameter)
endif ()
