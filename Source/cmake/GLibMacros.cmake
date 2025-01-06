macro(GENERATE_GLIB_API_HEADERS _framework _input_header_templates _derived_sources_directory _output_headers)
    foreach (header_template ${${_input_header_templates}})
        get_filename_component(header ${header_template} NAME_WLE)
        add_custom_command(
            OUTPUT ${_derived_sources_directory}/${header}
            DEPENDS ${header_template} ${WEBKIT_DIR}/Scripts/glib/generate-api-header.py ${UNIFDEF_EXECUTABLE}
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/glib/generate-api-header.py ${PORT} ${header_template} ${_derived_sources_directory}/${header} ${UNIFDEF_EXECUTABLE} ${ARGN}
            VERBATIM
        )
        list(APPEND ${_output_headers} ${_derived_sources_directory}/${header})
        unset(header)
    endforeach ()

    list(APPEND ${_framework}_SOURCES ${${_output_headers}})
endmacro()
