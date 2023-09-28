set(WebKitTestRunner_DIR "${TOOLS_DIR}/WebKitTestRunner")

file(MAKE_DIRECTORY ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle)
file(MAKE_DIRECTORY ${WebKitTestRunner_DERIVED_SOURCES_DIR}/UIScriptContext)

set(WebKitTestRunner_SOURCES
    CyclicRedundancyCheck.cpp
    GeolocationProviderMock.cpp
    Options.cpp
    PixelDumpSupport.cpp
    TestController.cpp
    TestInvocation.cpp
    TestOptions.cpp
    UIScriptControllerCommon.cpp
    WebNotificationProvider.cpp
    WorkQueueManager.cpp
)

set(WebKitTestRunner_LIBRARIES
    WebKit::TestRunnerShared
    WebKit::WebCoreTestSupport
)
set(WebKitTestRunner_FRAMEWORKS
    JavaScriptCore
    PAL
    WTF
    WebCore
    WebKit
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebKitTestRunner_FRAMEWORKS bmalloc)
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    if (HAVE_STD_EXPERIMENTAL_FILESYSTEM)
        list(APPEND WebKitTestRunner_LIBRARIES stdc++fs)
    endif ()
endif ()

set(WebKitTestRunner_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/UIScriptContext
    ${WebKitTestRunner_DIR}
)

set(WebKitTestRunner_WEB_PREFERENCES_TEMPLATES
    ${WebKitTestRunner_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb
)

set(WebKitTestRunner_WEB_PREFERENCES
    ${WTF_SCRIPTS_DIR}/Preferences/UnifiedWebPreferences.yaml
)

set_source_files_properties(${WebKitTestRunner_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${WebKitTestRunner_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
    DEPENDS ${WebKitTestRunner_WEB_PREFERENCES_TEMPLATES} ${WebKitTestRunner_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKit --outputDir "${WebKitTestRunner_DERIVED_SOURCES_DIR}" --template ${WebKitTestRunner_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb ${WebKitTestRunner_WEB_PREFERENCES}
    COMMAND_EXPAND_LISTS
    VERBATIM)

list(APPEND WebKitTestRunner_SOURCES
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
)

set(WebKitTestRunner_DEPENDENCIES TestRunnerInjectedBundle)

set(TestRunnerInjectedBundle_SOURCES
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

set(TestRunnerInjectedBundle_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle
    ${WebKitTestRunner_DIR}
    ${WebKitTestRunner_DIR}/InjectedBundle
)

set(TestRunnerInjectedBundle_LIBRARY_TYPE SHARED)
set(TestRunnerInjectedBundle_PRIVATE_LIBRARIES
    WebKit::TestRunnerShared
    WebKit::WebCoreTestSupport
)
set(TestRunnerInjectedBundle_FRAMEWORKS ${WebKitTestRunner_FRAMEWORKS})

set(TestRunnerInjectedBundle_IDL_FILES
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityController.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityTextMarker.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityTextMarkerRange.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/AccessibilityUIElement.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/EventSendingController.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/GCController.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/TestRunner.idl"
    "${WebKitTestRunner_DIR}/InjectedBundle/Bindings/TextInputController.idl"
)

GENERATE_BINDINGS(TestRunnerInjectedBundleBindings
    OUTPUT_SOURCE TestRunnerInjectedBundle_SOURCES
    INPUT_FILES ${TestRunnerInjectedBundle_IDL_FILES}
    BASE_DIR ${WebKitTestRunner_DIR}/InjectedBundle/Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle
    GENERATOR TestRunner
)

set(TestRunnerInjectedBundle_DEPENDENCIES
    TestRunnerInjectedBundleBindings
)

WEBKIT_EXECUTABLE_DECLARE(WebKitTestRunner)
WEBKIT_LIBRARY_DECLARE(TestRunnerInjectedBundle)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_LIBRARY(TestRunnerInjectedBundle)
WEBKIT_EXECUTABLE(WebKitTestRunner)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestRunnerInjectedBundle -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitTestRunner -Wno-unused-parameter)
endif ()
