include(WebKitCommon)

set_property(DIRECTORY . PROPERTY FOLDER "WebInspectorUI")

set(InspectorFiles
    ${WEBINSPECTORUI_DIR}/UserInterface/*.html
    ${WEBINSPECTORUI_DIR}/UserInterface/Base/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Controllers/*.css
    ${WEBINSPECTORUI_DIR}/UserInterface/Controllers/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Debug/*.css
    ${WEBINSPECTORUI_DIR}/UserInterface/Debug/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/External/CodeMirror/*.css
    ${WEBINSPECTORUI_DIR}/UserInterface/External/CodeMirror/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/External/Esprima/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/External/three.js/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Models/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Protocol/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Proxies/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Test/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Views/*.css
    ${WEBINSPECTORUI_DIR}/UserInterface/Views/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Workers/Formatter/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Workers/HeapSnapshot/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/Images/*.png
    ${WEBINSPECTORUI_DIR}/UserInterface/Images/*.svg
    ${WEBINSPECTORUI_DIR}/Localizations/en.lproj/localizedStrings.js
)

file(GLOB InspectorFilesDependencies
    ${InspectorFiles}
)

set(InspectorResourceScripts
    ${WEBINSPECTORUI_DIR}/Scripts/combine-resources.pl
    ${WEBINSPECTORUI_DIR}/Scripts/copy-user-interface-resources.pl
    ${WEBINSPECTORUI_DIR}/Scripts/fix-worker-imports-for-optimized-builds.pl
    ${WEBINSPECTORUI_DIR}/Scripts/remove-console-asserts.pl
    ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py
    ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
)

set(WebInspectorUI_RESOURCES_DIR "${WebInspectorUI_DERIVED_SOURCES_DIR}/InspectorResources")
set(WebInspectorUI_LOCALIZED_STRINGS_DIR "${WebInspectorUI_RESOURCES_DIR}/WebInspectorUI/Localizations/en.lproj")

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# DerivedSources/JavaScriptCore/inspector/InspectorBackendCommands.js is
# expected in DerivedSources/WebInspectorUI/UserInterface/Protocol/.
add_custom_command(
    OUTPUT ${WebInspectorUI_DERIVED_SOURCES_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    DEPENDS JavaScriptCore
            InspectorBackendCommands
            ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendCommands.js
    COMMAND ${CMAKE_COMMAND} -E copy ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendCommands.js ${WebInspectorUI_DERIVED_SOURCES_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    VERBATIM
)

if (NOT DEFINED COMBINE_INSPECTOR_RESOURCES)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(COMBINE_INSPECTOR_RESOURCES NO)
    else ()
        set(COMBINE_INSPECTOR_RESOURCES YES)
    endif ()
endif ()
if (NOT DEFINED COMBINE_TEST_RESOURCES)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR DEVELOPER_MODE)
        set(COMBINE_TEST_RESOURCES YES)
    else ()
        set(COMBINE_TEST_RESOURCES NO)
    endif ()
endif ()

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/inspector-resources.stamp
    DEPENDS ${InspectorFilesDependencies}
            ${InspectorResourceScripts}
            ${WebInspectorUI_DERIVED_SOURCES_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    COMMAND ${CMAKE_COMMAND} -E copy ${WebInspectorUI_DERIVED_SOURCES_DIR}/UserInterface/Protocol/InspectorBackendCommands.js ${JavaScriptCore_SCRIPTS_DIR}/InspectorBackendCommands.js
    COMMAND ${CMAKE_COMMAND} -E env "DERIVED_SOURCES_DIR=${WebInspectorUI_DERIVED_SOURCES_DIR}" "SRCROOT=${WEBINSPECTORUI_DIR}" "JAVASCRIPTCORE_PRIVATE_HEADERS_DIR=${JavaScriptCore_SCRIPTS_DIR}" "TARGET_BUILD_DIR=${WebInspectorUI_RESOURCES_DIR}" "UNLOCALIZED_RESOURCES_FOLDER_PATH=WebInspectorUI" "COMBINE_INSPECTOR_RESOURCES=${COMBINE_INSPECTOR_RESOURCES}" "COMBINE_TEST_RESOURCES=${COMBINE_TEST_RESOURCES}" PYTHON=${PYTHON_EXECUTABLE} ${PERL_EXECUTABLE} ${WEBINSPECTORUI_DIR}/Scripts/copy-user-interface-resources.pl
    COMMAND ${CMAKE_COMMAND} -E make_directory ${WebInspectorUI_LOCALIZED_STRINGS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy "${WEBINSPECTORUI_DIR}/Localizations/en.lproj/localizedStrings.js" ${WebInspectorUI_LOCALIZED_STRINGS_DIR}/localizedStrings.js
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/inspector-resources.stamp
    VERBATIM
)

add_custom_target(
    WebInspectorUI
    DEPENDS ${CMAKE_BINARY_DIR}/inspector-resources.stamp
)
