set_property(DIRECTORY . PROPERTY FOLDER "WebInspectorUI")

# FIXME: Mac CMake needs to additionally include image resources in PlatformMac.cmake.
file(GLOB_RECURSE WebInspectorUI_RESOURCES
    ${WEBINSPECTORUI_DIR}/UserInterface/*.html
    ${WEBINSPECTORUI_DIR}/UserInterface/*.js
    ${WEBINSPECTORUI_DIR}/UserInterface/*.css
)

set(WebInspectorUI_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Main.html
    ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Test.html
)
list(APPEND WebInspectorUI_RESOURCES ${WebInspectorUI_DERIVED_SOURCES})

# This variable is a hook for ports to toggle ENGINEERING_BUILD=1 based on some condition.
set(COMMON_RESOURCE_DEFINES)

# Copy InspectorBackendCommands.js from JavaScriptCore to WebInspectorUI.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    DEPENDS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js
    COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

set(MAIN_RESOURCE_DEFINES
    ${COMMON_RESOURCE_DEFINES}
    INCLUDE_UI_RESOURCES=1
)

set(TEST_RESOURCE_DEFINES
    ${COMMON_RESOURCE_DEFINES}
    INCLUDE_TEST_RESOURCES=1
)

macro(PROCESS_INSPECTOR_MAIN_RESOURCE _defines _out_file)
    TO_LIST_WITH_SPACES("${_defines}" _defines_with_space_separator)
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/${_out_file}
        MAIN_DEPENDENCY ${WEBINSPECTORUI_DIR}/Scripts/preprocess-main-resource.pl
        DEPENDS ${WEBINSPECTORUI_DIR}/UserInterface/Inspector.html.in
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${WEBINSPECTORUI_DIR}/Scripts/preprocess-main-resource.pl --defines="${_defines_with_space_separator}" ${WEBINSPECTORUI_DIR}/UserInterface/Inspector.html.in > ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/${_out_file}
    )
endmacro()

PROCESS_INSPECTOR_MAIN_RESOURCE("${MAIN_RESOURCE_DEFINES}" Main.html)
PROCESS_INSPECTOR_MAIN_RESOURCE("${TEST_RESOURCE_DEFINES}" Test.html)

# Force generation of derived sources here since they are eventually needed by WebKit2.
# They can't be generated in WebKit2 because WebInspectorUI_RESOURCES is out of scope.
add_custom_target(generate-inspector-resources ALL
    DEPENDS ${WebInspectorUI_RESOURCES}
)
