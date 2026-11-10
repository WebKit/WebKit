set_property(DIRECTORY . PROPERTY FOLDER "WebInspectorUI")

set(InspectorResourceScripts
    ${WEBINSPECTORUI_DIR}/Scripts/combine-resources.pl
    ${WEBINSPECTORUI_DIR}/Scripts/copy-user-interface-resources.pl
    ${WEBINSPECTORUI_DIR}/Scripts/fix-worker-imports-for-optimized-builds.pl
    ${WEBINSPECTORUI_DIR}/Scripts/remove-console-asserts.pl
    ${WEBINSPECTORUI_DIR}/Scripts/update-NativeFunctionParameters.py
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
    DEPENDS ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendCommands.js
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendCommands.js ${WebInspectorUI_DERIVED_SOURCES_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    VERBATIM
)

if (NOT DEFINED COMBINE_INSPECTOR_RESOURCES)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR DEVELOPER_MODE)
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

if (NOT DEFINED INCLUDE_BROWSER_INSPECTOR_FRONTEND_HOST)
    set(INCLUDE_BROWSER_INSPECTOR_FRONTEND_HOST NO)
endif ()

# copy-user-interface-resources.pl walks UserInterface/ itself, so a depfile
# listing everything there stands in for the tree as a dependency.
webkit_get_directory_depfile_command(InspectorFilesDepfileCommand
    OUTPUT ${CMAKE_BINARY_DIR}/inspector-resources.stamp
    DEPFILE ${CMAKE_BINARY_DIR}/inspector-resources.stamp.d
    DIRECTORIES ${WEBINSPECTORUI_DIR}/UserInterface ${WEBINSPECTORUI_DIR}/Localizations
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/inspector-resources.stamp
    DEPENDS ${InspectorResourceScripts}
            ${WebInspectorUI_DERIVED_SOURCES_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
            ${WebCore_PRIVATE_HEADERS_DIR}/InspectorNativeFunctionParameters.json
            ${WEBINSPECTORUI_DIR}/UserInterface/Models/NativeFunctionParameters-overrides.json
    COMMAND ${CMAKE_COMMAND} -E copy ${WebInspectorUI_DERIVED_SOURCES_DIR}/UserInterface/Protocol/InspectorBackendCommands.js ${JavaScriptCore_SCRIPTS_DIR}/InspectorBackendCommands.js
    COMMAND ${CMAKE_COMMAND} -E env "DERIVED_SOURCES_DIR=${WebInspectorUI_DERIVED_SOURCES_DIR}" "SRCROOT=${WEBINSPECTORUI_DIR}" "JAVASCRIPTCORE_PRIVATE_HEADERS_DIR=${JavaScriptCore_SCRIPTS_DIR}" "WEBCORE_PRIVATE_HEADERS_DIR=${WebCore_PRIVATE_HEADERS_DIR}" "TARGET_BUILD_DIR=${WebInspectorUI_RESOURCES_DIR}" "UNLOCALIZED_RESOURCES_FOLDER_PATH=WebInspectorUI" "COMBINE_INSPECTOR_RESOURCES=${COMBINE_INSPECTOR_RESOURCES}" "COMBINE_TEST_RESOURCES=${COMBINE_TEST_RESOURCES}" "INCLUDE_BROWSER_INSPECTOR_FRONTEND_HOST=${INCLUDE_BROWSER_INSPECTOR_FRONTEND_HOST}" PYTHON=${PYTHON_EXECUTABLE} ${PERL_EXECUTABLE} ${WEBINSPECTORUI_DIR}/Scripts/copy-user-interface-resources.pl
    COMMAND ${CMAKE_COMMAND} -E make_directory ${WebInspectorUI_LOCALIZED_STRINGS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy "${WEBINSPECTORUI_DIR}/Localizations/en.lproj/localizedStrings.js" ${WebInspectorUI_LOCALIZED_STRINGS_DIR}/localizedStrings.js
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/inspector-resources.stamp
    COMMAND ${InspectorFilesDepfileCommand}
    DEPFILE ${CMAKE_BINARY_DIR}/inspector-resources.stamp.d
    VERBATIM
)

add_custom_target(
    WebInspectorUI
    DEPENDS ${CMAKE_BINARY_DIR}/inspector-resources.stamp
)
