set(DumpRenderTree_DIR "${TOOLS_DIR}/DumpRenderTree")
set(DumpRenderTree_BINDINGS_DIR "${TOOLS_DIR}/DumpRenderTree/Bindings")

file(MAKE_DIRECTORY ${DumpRenderTree_DERIVED_SOURCES_DIR})

set(DumpRenderTree_SOURCES
    AccessibilityController.cpp
    AccessibilityTextMarker.cpp
    AccessibilityUIElement.cpp
    CyclicRedundancyCheck.cpp
    GCController.cpp
    JavaScriptThreading.cpp
    PixelDumpSupport.cpp
    TestOptions.cpp
    TestRunner.cpp
    WorkQueue.cpp
)

set(DumpRenderTree_FRAMEWORKS
    JavaScriptCore
    PAL
    TestRunnerShared
    WTF
    WebCore
    WebCoreTestSupport
    WebKitLegacy
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebKitTestRunner_FRAMEWORKS bmalloc)
endif ()

set(DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${DumpRenderTree_DERIVED_SOURCES_DIR}
    ${DumpRenderTree_DIR}
)

set(DumpRenderTree_WEB_PREFERENCES_TEMPLATES
    ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp.erb
    ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb
)

set(DumpRenderTree_WEB_PREFERENCES
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferences.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml
)

set_source_files_properties(${DumpRenderTree_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
    DEPENDS ${DumpRenderTree_WEB_PREFERENCES_TEMPLATES} ${DumpRenderTree_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKitLegacy --base ${WTF_SCRIPTS_DIR}/Preferences/WebPreferences.yaml --debug ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml --experimental ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml --internal ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml --outputDir "${DumpRenderTree_DERIVED_SOURCES_DIR}" --template ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp.erb --template ${DumpRenderTree_DIR}/Scripts/PreferencesTemplates/TestOptionsGeneratedKeys.h.erb
    VERBATIM)

list(APPEND DumpRenderTree_SOURCES
    ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedWebKitLegacyKeyMapping.cpp
    ${DumpRenderTree_DERIVED_SOURCES_DIR}/TestOptionsGeneratedKeys.h
)

WEBKIT_EXECUTABLE_DECLARE(DumpRenderTree)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_EXECUTABLE(DumpRenderTree)
