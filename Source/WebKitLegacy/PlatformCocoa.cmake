# FIXME: Remove once source files are fixed. https://bugs.webkit.org/show_bug.cgi?id=312034
WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitLegacy -Wno-unused-parameter)

WEBKIT_ADD_PREFIX_HEADER(WebKitLegacy WebKitLegacyPrefix.h PREFIX_LANGUAGES CXX OBJC OBJCXX)

list(APPEND WebKitLegacy_PRIVATE_LIBRARIES
    PAL
)

list(APPEND WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_FRAMEWORK_HEADERS_DIR}"
    "${WEBKITLEGACY_DIR}"
    "${WEBKITLEGACY_DIR}/mac"
    "${WEBKITLEGACY_DIR}/mac/Misc"
    "${WEBKITLEGACY_DIR}/mac/WebView"
    "${WEBKITLEGACY_DIR}/mac/WebCoreSupport"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/abseil-cpp"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/webrtc"
)

list(APPEND WebKitLegacy_UNIFIED_SOURCE_LIST_FILES
    SourcesCocoa.txt
)
# FIXME: Test building on iOS and then enable on iOS.
if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND WebKitLegacy_UNIFIED_SOURCE_LIST_FILES
        SourcesCMakeCocoa.txt
    )
endif ()
WEBKIT_COMPUTE_SOURCES(WebKitLegacy)

list(APPEND WebKitLegacy_SOURCES
    WebCoreSupport/LegacySocketProvider.cpp
    WebCoreSupport/LegacyWebPageDebuggable.cpp
    WebCoreSupport/LegacyWebPageInspectorController.cpp
    WebCoreSupport/WebCryptoClient.mm

    cf/WebCoreSupport/WebInspectorClientCF.cpp
)

# Preferences codegen.
set(WebKitLegacy_WEB_PREFERENCES_TEMPLATES
    ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebViewPreferencesChangedGenerated.mm.erb
    ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebPreferencesInternalFeatures.mm.erb
    ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebPreferencesExperimentalFeatures.mm.erb
    ${WEBKITLEGACY_DIR}/mac/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb
)
set(WebKitLegacy_WEB_PREFERENCES ${WTF_SCRIPTS_DIR}/Preferences/UnifiedWebPreferences.yaml)
set_source_files_properties(${WebKitLegacy_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebViewPreferencesChangedGenerated.mm ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesInternalFeatures.mm ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.mm ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h
    DEPENDS ${WebKitLegacy_WEB_PREFERENCES_TEMPLATES} ${WebKitLegacy_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${Ruby_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKitLegacy --outputDir "${WebKitLegacy_DERIVED_SOURCES_DIR}" --template "$<JOIN:${WebKitLegacy_WEB_PREFERENCES_TEMPLATES},;--template;>" ${WebKitLegacy_WEB_PREFERENCES}
    COMMAND_EXPAND_LISTS
    VERBATIM
)

list(APPEND WebKitLegacy_SOURCES
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebViewPreferencesChangedGenerated.mm
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesInternalFeatures.mm
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.mm
)

set(WebKitLegacy_OUTPUT_NAME WebKitLegacy)
