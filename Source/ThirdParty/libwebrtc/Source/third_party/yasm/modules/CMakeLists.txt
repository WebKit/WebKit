INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

INCLUDE(arch/CMakeLists.txt)
INCLUDE(listfmts/CMakeLists.txt)
INCLUDE(parsers/CMakeLists.txt)
INCLUDE(preprocs/CMakeLists.txt)
INCLUDE(dbgfmts/CMakeLists.txt)
INCLUDE(objfmts/CMakeLists.txt)

MESSAGE(STATUS "Standard modules: ${YASM_MODULES}")

#
# Generate init_plugin.c
# This file provides the yasm_init_plugin() function for yasmstd.
#

SET(INIT_PLUGIN_C ${CMAKE_CURRENT_BINARY_DIR}/init_plugin.c)
SET(INIT_PLUGIN_C_REV 1)

# Don't regen if no changes; default to regen
SET(regen_init_plugin_c TRUE)
IF(EXISTS ${INIT_PLUGIN_C})
    FILE(READ ${INIT_PLUGIN_C} _old_init_plugin_c)
    STRING(REGEX MATCHALL "[^\n]*\n" _lines "${_old_init_plugin_c}")
    #MESSAGE(STATUS "Lines: ${_lines}")

    LIST(GET _lines 0 _line0)
    STRING(REGEX MATCH "([A-Za-z][A-Za-z0-9_]+[ ]?)+" _old_modules "${_line0}")
    #MESSAGE(STATUS "Old modules: ${_old_modules}")
    STRING(REPLACE ";" " " _modules_str "${YASM_MODULES}")
    STRING(COMPARE EQUAL "${_old_modules}" "${_modules_str} " _modules_match)

    LIST(GET _lines 1 _line1)
    STRING(REGEX MATCH "rev [0-9]+" _old_modules_rev "${_line1}")
    #MESSAGE(STATUS "Old modules rev: ${_old_modules_rev}")
    STRING(COMPARE EQUAL "${_old_modules_rev}" "rev ${INIT_PLUGIN_C_REV}"
           _modules_rev_match)

    IF(_modules_match AND _modules_rev_match)
        SET(regen_init_plugin_c FALSE)
    ENDIF(_modules_match AND _modules_rev_match)
ENDIF(EXISTS ${INIT_PLUGIN_C})

IF(regen_init_plugin_c)
    MESSAGE(STATUS "Generating standard plugin initialization file")
    STRING(REPLACE ";" " " _modules_str "${YASM_MODULES}")
    FILE(WRITE ${INIT_PLUGIN_C} "/* ${_modules_str} \n")
    FILE(APPEND ${INIT_PLUGIN_C} "   rev ${INIT_PLUGIN_C_REV}\n")
    FILE(APPEND ${INIT_PLUGIN_C} " */\n\n")
    FILE(APPEND ${INIT_PLUGIN_C} "#include <libyasm.h>\n")
    FILE(APPEND ${INIT_PLUGIN_C} "#include <libyasm/module.h>\n\n")
    FOREACH(module ${YASM_MODULES})
        STRING(REGEX MATCHALL "[a-zA-Z][a-zA-Z0-9]+" _modulepath ${module})
        LIST(GET _modulepath 0 _type)
        LIST(GET _modulepath 1 _keyword)
        FILE(APPEND ${INIT_PLUGIN_C}
             "extern yasm_${_type}_module yasm_${_keyword}_LTX_${_type};\n")
    ENDFOREACH(module)
    IF(BUILD_SHARED_LIBS)
        FILE(APPEND ${INIT_PLUGIN_C} "\n#ifdef _MSC_VER\n")
        FILE(APPEND ${INIT_PLUGIN_C} "__declspec(dllexport)\n")
        FILE(APPEND ${INIT_PLUGIN_C} "#endif\n")
    ENDIF(BUILD_SHARED_LIBS)
    FILE(APPEND ${INIT_PLUGIN_C} "void\n")
    FILE(APPEND ${INIT_PLUGIN_C} "yasm_init_plugin(void)\n")
    FILE(APPEND ${INIT_PLUGIN_C} "{\n")
    FOREACH(module ${YASM_MODULES})
        STRING(REGEX MATCHALL "[a-zA-Z][a-zA-Z0-9]+" _modulepath ${module})
        LIST(GET _modulepath 0 _type)
        LIST(GET _modulepath 1 _keyword)
        SET(_data "yasm_${_keyword}_LTX_${_type}")
        STRING(TOUPPER "${_type}" _type)
        FILE(APPEND ${INIT_PLUGIN_C}
             "    yasm_register_module(YASM_MODULE_${_type}, \"${_keyword}\", &${_data});\n")
    ENDFOREACH(module)
    FILE(APPEND ${INIT_PLUGIN_C} "}\n") 
ELSE(regen_init_plugin_c)
    MESSAGE(STATUS "Not regenerating static modules file (unchanged)")
ENDIF(regen_init_plugin_c)

SET_SOURCE_FILES_PROPERTIES(init_plugin.c GENERATED)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

IF(BUILD_SHARED_LIBS)
    ADD_LIBRARY(yasmstd MODULE
        init_plugin.c
        ${YASM_MODULES_SRC}
        )
    TARGET_LINK_LIBRARIES(yasmstd libyasm)

    IF(WIN32)
        INSTALL(TARGETS yasmstd LIBRARY DESTINATION bin)
    ELSE(WIN32)
        INSTALL(TARGETS yasmstd LIBRARY DESTINATION lib)
    ENDIF(WIN32)
ELSE(BUILD_SHARED_LIBS)
    ADD_LIBRARY(yasmstd
        init_plugin.c
        ${YASM_MODULES_SRC}
        )
    TARGET_LINK_LIBRARIES(yasmstd libyasm)
ENDIF(BUILD_SHARED_LIBS)
