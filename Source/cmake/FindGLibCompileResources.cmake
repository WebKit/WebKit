pkg_get_variable(GLIB2_PREFIX glib-2.0 prefix)
find_program(
    GLIB_COMPILE_RESOURCES_EXECUTABLE
    NAMES glib-compile-resources
    HINTS ${GLIB2_PREFIX}
    REQUIRED
)
execute_process(
    COMMAND ${GLIB_COMPILE_RESOURCES_EXECUTABLE} --version
    OUTPUT_VARIABLE glib_compile_resources_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found glib-compile-resources (required): "
    ${GLIB_COMPILE_RESOURCES_EXECUTABLE}
    " (${glib_compile_resources_version})"
)

set(glib_compile_resources_has_depfile_bug FALSE)
if ("${glib_compile_resources_version}" VERSION_LESS 2.77)
    set(glib_compile_resources_has_depfile_bug TRUE)
endif ()

function(GLIB_COMPILE_RESOURCES)
    set(zeroArgKeywords "")
    set(oneArgKeywords OUTPUT SOURCE_XML)
    set(manyArgsKeywords RESOURCE_DIRS)
    cmake_parse_arguments(
        PARSE_ARGV 0 ARG
        "${zeroArgKeywords}" "${oneArgKeywords}" "${manyArgsKeywords}"
    )

    set(resource_dir_args "")
    foreach (resource_dir IN LISTS ARG_RESOURCE_DIRS)
        list(APPEND resource_dir_args --sourcedir=${resource_dir})
    endforeach ()

    set(additional_cmd_line)
    if (${glib_compile_resources_has_depfile_bug})
        # Workaround for older versions of glib-compile-resources lacking this fix:
        # https://gitlab.gnome.org/GNOME/glib/-/merge_requests/3460
        #
        # Affected versions produce broken depfiles that look like this:
        #   foo.xml: resource1 resource2
        # But depfiles should look like this:
        #   foo.c: foo.xml resource1 resource2
        set(additional_cmd_line &&
            ${PERL_EXECUTABLE} -pi ${CMAKE_SOURCE_DIR}/Tools/glib/fix-glib-resources-depfile.pl
            ${ARG_SOURCE_XML} ${ARG_OUTPUT} ${ARG_OUTPUT}.deps)
    endif ()

    add_custom_command(
        OUTPUT  ${ARG_OUTPUT} ${ARG_OUTPUT}.deps
        DEPENDS ${ARG_SOURCE_XML}
        DEPFILE ${ARG_OUTPUT}.deps
        COMMAND ${GLIB_COMPILE_RESOURCES_EXECUTABLE}
                --generate
                --target=${ARG_OUTPUT}
                --dependency-file=${ARG_OUTPUT}.deps
                ${resource_dir_args}
                ${ARG_SOURCE_XML}
                ${additional_cmd_line}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        VERBATIM
    )
endfunction()