set(WEBKIT_TESTRUNNER_DIR "${TOOLS_DIR}/WebKitTestRunner")
set(WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR "${TOOLS_DIR}/WebKitTestRunner/InjectedBundle")
set(WEBKIT_TESTRUNNER_SHARED_DIR "${TOOLS_DIR}/TestRunnerShared/")
set(WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR "${WEBKIT_TESTRUNNER_SHARED_DIR}/UIScriptContext")
set(WEBKIT_TESTRUNNER_BINDINGS_DIR "${WEBKIT_TESTRUNNER_SHARED_DIR}/Bindings")

file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/InjectedBundle)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/UIScriptContext)

set(WebKitTestRunner_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/CyclicRedundancyCheck.cpp
    ${WEBKIT_TESTRUNNER_DIR}/GeolocationProviderMock.cpp
    ${WEBKIT_TESTRUNNER_DIR}/Options.cpp
    ${WEBKIT_TESTRUNNER_DIR}/PixelDumpSupport.cpp
    ${WEBKIT_TESTRUNNER_DIR}/TestController.cpp
    ${WEBKIT_TESTRUNNER_DIR}/TestInvocation.cpp
    ${WEBKIT_TESTRUNNER_DIR}/TestOptions.cpp
    ${WEBKIT_TESTRUNNER_DIR}/WebNotificationProvider.cpp
    ${WEBKIT_TESTRUNNER_DIR}/WorkQueueManager.cpp
    ${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}/UIScriptContext.cpp
    ${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}/UIScriptController.cpp
    ${WEBKIT_TESTRUNNER_BINDINGS_DIR}/JSWrapper.cpp
)

set(WebKitTestRunner_LIBRARIES
    JavaScriptCore
    WebCoreTestSupport
    WebKit
)

set(WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WEBKIT_TESTRUNNER_DIR}
    ${WEBKIT_TESTRUNNER_DIR}/InjectedBundle
    ${WEBKIT_TESTRUNNER_DIR}/InjectedBundle/Bindings
    ${WEBKIT_TESTRUNNER_DIR}/InjectedBundle/atk
    ${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}
    ${WEBKIT_TESTRUNNER_BINDINGS_DIR}
    ${WEBCORE_DIR}/testing/js
    ${WEBKIT_DIR}/Platform/IPC
    ${WEBKIT_DIR}/Shared
    ${WEBKIT_DIR}/Shared/API/c
    ${WEBKIT_DIR}/Shared/Plugins
    ${WEBKIT_DIR}/UIProcess
    ${WEBKIT_DIR}/UIProcess/API/C/soup
    ${WEBKIT_DIR}/WebProcess/InjectedBundle
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c
    ${DERIVED_SOURCES_DIR}/InjectedBundle
    ${DERIVED_SOURCES_DIR}/UIScriptContext
    ${CMAKE_SOURCE_DIR}/Source
)

set(WebKitTestRunner_SYSTEM_INCLUDE_DIRECTORIES
    ${LIBSOUP_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
)

set(WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/EventSendingController.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/GCController.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/InjectedBundle.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/InjectedBundleMain.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/InjectedBundlePage.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/TestRunner.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/TextInputController.cpp

    ${WEBKIT_TESTRUNNER_BINDINGS_DIR}/JSWrapper.cpp
)

set(WebKitTestRunnerInjectedBundle_IDL_FILES
    "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/EventSendingController.idl"
    "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/GCController.idl"
    "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/TestRunner.idl"
    "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/TextInputController.idl"
)

if (ENABLE_ACCESSIBILITY)
    list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
        ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityController.cpp
        ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityTextMarker.cpp
        ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityTextMarkerRange.cpp
        ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityUIElement.cpp
    )

    list(APPEND WebKitTestRunnerInjectedBundle_IDL_FILES
        "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/AccessibilityController.idl"
        "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/AccessibilityTextMarker.idl"
        "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/AccessibilityTextMarkerRange.idl"
        "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/AccessibilityUIElement.idl"
    )
endif ()

set(WebKitTestRunner_IDL_FILES
    "${WEBKIT_TESTRUNNER_UISCRIPTCONTEXT_DIR}/Bindings/UIScriptController.idl"
)

GENERATE_BINDINGS(WebKitTestRunnerInjectedBundleBindings
    OUTPUT_SOURCE WebKitTestRunnerInjectedBundle_SOURCES
    INPUT_FILES ${WebKitTestRunnerInjectedBundle_IDL_FILES}
    BASE_DIR ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings
    IDL_INCLUDES ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${DERIVED_SOURCES_DIR}/InjectedBundle
    GENERATOR TestRunner)

GENERATE_BINDINGS(WebKitTestRunnerBindings
    OUTPUT_SOURCE WebKitTestRunner_SOURCES
    INPUT_FILES ${WebKitTestRunner_IDL_FILES}
    BASE_DIR ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings
    IDL_INCLUDES ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${DERIVED_SOURCES_DIR}/UIScriptContext
    GENERATOR TestRunner)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(${WebKitTestRunner_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${WebKitTestRunner_SYSTEM_INCLUDE_DIRECTORIES})

add_library(TestRunnerInjectedBundle SHARED ${WebKitTestRunnerInjectedBundle_SOURCES})
target_link_libraries(TestRunnerInjectedBundle ${WebKitTestRunnerInjectedBundle_LIBRARIES})
add_dependencies(TestRunnerInjectedBundle WebKitTestRunnerInjectedBundleBindings)

add_executable(WebKitTestRunner ${WebKitTestRunner_SOURCES})
target_link_libraries(WebKitTestRunner ${WebKitTestRunner_LIBRARIES})
add_dependencies(WebKitTestRunner WebKitTestRunnerBindings)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestRunnerInjectedBundle -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitTestRunner -Wno-unused-parameter)
endif ()

if (WIN32)
    add_dependencies(WebKitTestRunnerLib WebKitTestRunnerBindings)
    add_dependencies(WebKitTestRunner WebKitTestRunnerLib)
elseif (NOT APPLE)
    add_dependencies(WebKit ${ForwardingHeadersForWebKitTestRunner_NAME})
endif ()
