include(../ThirdParty/unifdef/CMakeLists.txt)

macro(GENERATE_API_HEADERS _input_header_templates _derived_sources_directory _output_headers)
    foreach (header_template ${${_input_header_templates}})
        get_filename_component(header ${header_template} NAME_WLE)
        configure_file(${header_template} ${_derived_sources_directory}/${header}.tmp)
        add_custom_command(
            OUTPUT ${_derived_sources_directory}/${header}
            DEPENDS ${header_template} unifdef
            COMMAND unifdef ${ARGN} -x2 -o ${_derived_sources_directory}/${header} ${_derived_sources_directory}/${header}.tmp
            VERBATIM
        )
        list(APPEND ${_output_headers} "${_derived_sources_directory}/${header}")
        unset(header)
    endforeach ()

    list(APPEND WebKit_SOURCES ${${_output_headers}})
endmacro()
