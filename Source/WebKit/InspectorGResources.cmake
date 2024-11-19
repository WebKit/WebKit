macro(WEBKIT_BUILD_INSPECTOR_GRESOURCES _derived_sources_dir _output_file)
    add_custom_command(
        OUTPUT ${_derived_sources_dir}/InspectorGResourceBundle.xml
        DEPENDS WebInspectorUI
                ${CMAKE_BINARY_DIR}/inspector-resources.stamp
                ${TOOLS_DIR}/glib/generate-inspector-gresource-manifest.py
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-inspector-gresource-manifest.py --input=${_derived_sources_dir}/InspectorResources --output=${_derived_sources_dir}/InspectorGResourceBundle.xml
        VERBATIM
    )

    GLIB_COMPILE_RESOURCES(
        OUTPUT        ${_output_file}
        SOURCE_XML    ${_derived_sources_dir}/InspectorGResourceBundle.xml
        RESOURCE_DIRS ${_derived_sources_dir}/InspectorResources/WebInspectorUI
    )

    add_custom_target(InspectorResources
        ALL
        DEPENDS ${_output_file}
    )
endmacro()
