macro(WEBKIT_BUILD_INSPECTOR_GRESOURCES _derived_sources_dir)
    set(InspectorFiles
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/*.html
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Base/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.css
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Debug/*.css
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Debug/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/CodeMirror/*.css
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/CodeMirror/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/Esprima/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/three.js/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Models/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Protocol/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Proxies/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Test/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.css
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/Formatter/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/HeapSnapshot/*.js
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/*.png
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/*.svg
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Localizations/en.lproj/localizedStrings.js
    )

    file(GLOB InspectorFilesDependencies
        ${InspectorFiles}
    )

    set(InspectorResourceScripts
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Scripts/combine-resources.pl
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Scripts/copy-user-interface-resources.pl
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Scripts/fix-worker-imports-for-optimized-builds.pl
        ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Scripts/remove-console-asserts.pl
        ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py
        ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    )

    # DerivedSources/JavaScriptCore/inspector/InspectorBackendCommands.js is
    # expected in DerivedSources/WebInspectorUI/UserInterface/Protocol/.
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
        DEPENDS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js
        COMMAND cp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
        VERBATIM
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(COMBINE_INSPECTOR_RESOURCES NO)
        set(COMBINE_TEST_RESOURCES YES)
    elseif (DEVELOPER_MODE)
        set(COMBINE_INSPECTOR_RESOURCES YES)
        set(COMBINE_TEST_RESOURCES YES)
    else ()
        set(COMBINE_INSPECTOR_RESOURCES YES)
        set(COMBINE_TEST_RESOURCES NO)
    endif ()

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/inspector-resources.stamp
        DEPENDS ${InspectorFilesDependencies}
                ${InspectorResourceScripts}
                ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
        COMMAND cp ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js ${FORWARDING_HEADERS_DIR}/JavaScriptCore/Scripts
        COMMAND ${CMAKE_COMMAND} -E env "DERIVED_SOURCES_DIR=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}" "SRCROOT=${CMAKE_SOURCE_DIR}/Source/WebInspectorUI" "JAVASCRIPTCORE_PRIVATE_HEADERS_DIR=${FORWARDING_HEADERS_DIR}/JavaScriptCore/Scripts" "TARGET_BUILD_DIR=${_derived_sources_dir}/InspectorResources" "UNLOCALIZED_RESOURCES_FOLDER_PATH=WebInspectorUI" "COMBINE_INSPECTOR_RESOURCES=${COMBINE_INSPECTOR_RESOURCES}" "COMBINE_TEST_RESOURCES=${COMBINE_TEST_RESOURCES}" ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Scripts/copy-user-interface-resources.pl
        COMMAND mkdir -p ${_derived_sources_dir}/InspectorResources/WebInspectorUI/Localizations/en.lproj
        COMMAND cp ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Localizations/en.lproj/localizedStrings.js ${_derived_sources_dir}/InspectorResources/WebInspectorUI/Localizations/en.lproj/localizedStrings.js
        COMMAND touch ${CMAKE_BINARY_DIR}/inspector-resources.stamp
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/InspectorGResourceBundle.xml
        DEPENDS ${CMAKE_BINARY_DIR}/inspector-resources.stamp
                ${TOOLS_DIR}/glib/generate-inspector-gresource-manifest.py
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-inspector-gresource-manifest.py --input=${_derived_sources_dir}/InspectorResources --output=${_derived_sources_dir}/InspectorGResourceBundle.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/InspectorGResourceBundle.c
        DEPENDS ${_derived_sources_dir}/InspectorGResourceBundle.xml
        COMMAND glib-compile-resources --generate --sourcedir=${_derived_sources_dir}/InspectorResources/WebInspectorUI --target=${_derived_sources_dir}/InspectorGResourceBundle.c ${_derived_sources_dir}/InspectorGResourceBundle.xml
        VERBATIM
    )
endmacro()
