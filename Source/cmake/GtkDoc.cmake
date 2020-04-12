# Calls scripts to generate documentation using GTK-Doc.
# DocumentationDependencies variable must be set before calling this macro.
macro(ADD_GTKDOC_GENERATOR _stamp_name _extra_args)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/${_stamp_name}"
        DEPENDS ${DocumentationDependencies}
        COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}" "CFLAGS=${CMAKE_C_FLAGS} -Wno-unused-parameter" "LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}" ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Tools/gtkdoc/generate-gtkdoc ${_extra_args}
        COMMAND touch ${_stamp_name}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        VERBATIM
    )
endmacro()
