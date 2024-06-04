macro(GENERATE_GLIB_API_HEADERS _framework _input_header_templates _derived_sources_directory _output_headers)
    foreach (header_template ${${_input_header_templates}})
        get_filename_component(header ${header_template} NAME_WLE)
        add_custom_command(
            OUTPUT ${_derived_sources_directory}/${header}
            DEPENDS ${header_template} ${WEBKIT_DIR}/Scripts/glib/generate-api-header.py
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/glib/generate-api-header.py ${PORT} ${header_template} ${_derived_sources_directory}/${header} ${UNIFDEF_EXECUTABLE} ${ARGN}
            VERBATIM
        )
        list(APPEND ${_output_headers} ${_derived_sources_directory}/${header})
        unset(header)
    endforeach ()

    list(APPEND ${_framework}_SOURCES ${${_output_headers}})
endmacro()

function(GENERATE_LINKER_SCRIPT target template)
    if (DEVELOPER_MODE OR CMAKE_SYSTEM_NAME MATCHES "Darwin")
        return()
    endif ()

    set(linker_script_target "${target}-linker-script")
    set(linker_script "${CMAKE_CURRENT_BINARY_DIR}/${target}-symbols.map")

    add_custom_command(
        OUTPUT "${linker_script}"
        DEPENDS "${template}"
        COMMAND "${UNIFDEF_EXECUTABLE}" -x1 -t "-o${linker_script}"
            ${ARGN} "${template}"
        VERBATIM
    )
    target_link_options(${target} PRIVATE "-Wl,--version-script,${linker_script}")

    #
    # There is a LINK_DEPENDS target property, but the files listed do not
    # nudge CMake into materializing the build rules for custom commands;
    # it only works with preexisting files. Workaround by adding a dummy
    # custom target and depend on that to force the rules to be generated.
    #
    # See https://discourse.cmake.org/t/link-depends-on-output-of-add-custom-command-no-rule-to-make-target/10179
    #
    add_custom_target(${linker_script_target} DEPENDS "${linker_script}")
    add_dependencies(${target} ${linker_script_target})
endfunction()
