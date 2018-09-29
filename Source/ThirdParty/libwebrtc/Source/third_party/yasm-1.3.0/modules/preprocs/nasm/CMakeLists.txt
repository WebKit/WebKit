add_executable(genversion preprocs/nasm/genversion.c)
get_target_property(_tmp_GENVERSION_EXE genversion LOCATION)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.mac
    COMMAND ${_tmp_GENVERSION_EXE} ${CMAKE_CURRENT_BINARY_DIR}/version.mac
    DEPENDS ${_tmp_GENVERSION_EXE}
    )

YASM_GENMACRO(
    ${CMAKE_CURRENT_BINARY_DIR}/version.mac
    ${CMAKE_CURRENT_BINARY_DIR}/nasm-version.c
    nasm_version_mac
    )

SET_SOURCE_FILES_PROPERTIES(preprocs/nasm/nasm-preproc.c PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/nasm-version.c
    )

YASM_ADD_MODULE(preproc_nasm
    preprocs/nasm/nasm-preproc.c
    preprocs/nasm/nasm-pp.c
    preprocs/nasm/nasmlib.c
    preprocs/nasm/nasm-eval.c
    )

