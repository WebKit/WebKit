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

set(WebKitTestRunner_FRAMEWORKS
    JavaScriptCore
    PAL
    TestRunnerShared
    WTF
    WebCore
    WebCoreTestSupport
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebKitTestRunner_FRAMEWORKS bmalloc)
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    if (HAVE_STD_EXPERIMENTAL_FILESYSTEM)
        list(APPEND WebKitTestRunner_LIBRARIES stdc++fs)
    endif ()
endif ()

set(WebKitTestRunner_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/UIScriptContext
    ${WebKitTestRunner_DIR}
)

set(WebKitTestRunner_WEB_PREFERENCES_TEMPLATES
    ${WebKitTestRunner_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb
)

set(WebKitTestRunner_WEB_PREFERENCES
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferences.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml
)

set_source_files_properties(${WebKitTestRunner_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${WebKitTestRunner_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
    DEPENDS ${WebKitTestRunner_WEB_PREFERENCES_TEMPLATES} ${WebKitTestRunner_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKit --base ${WTF_SCRIPTS_DIR}/Preferences/WebPreferences.yaml --debug ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml --experimental ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml --internal ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml --outputDir "${WebKitTestRunner_DERIVED_SOURCES_DIR}" --template ${WebKitTestRunner_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb
    VERBATIM)

list(APPEND WebKitTestRunner_SOURCES
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
)

set(WebKitTestRunnerInjectedBundle_SOURCES
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
    ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle
    ${WebKitTestRunner_DIR}
    ${WebKitTestRunner_DIR}/InjectedBundle
)

set(WebKitTestRunnerInjectedBundle_LIBRARIES
    WebKit::TestRunnerShared
    WebKit::WebCore
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

set(WebKitTestRunner_DEPENDENCIES
    TestRunnerInjectedBundle
)

GENERATE_BINDINGS(WebKitTestRunnerInjectedBundleBindings
    OUTPUT_SOURCE WebKitTestRunnerInjectedBundle_SOURCES
    INPUT_FILES ${WebKitTestRunnerInjectedBundle_IDL_FILES}
    BASE_DIR ${WebKitTestRunner_DIR}/InjectedBundle/Bindings
    IDL_INCLUDES ${WebKitTestRunner_DIR}/InjectedBundle/Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${WebKitTestRunner_DERIVED_SOURCES_DIR}/InjectedBundle
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
