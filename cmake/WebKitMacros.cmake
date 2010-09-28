MACRO (INCLUDE_IF_EXISTS _file)
    IF (EXISTS ${_file})
        MESSAGE(STATUS "Using platform-specific CMakeLists: ${_file}")
        INCLUDE(${_file})
    ELSE ()
        MESSAGE(STATUS "Platform-specific CMakeLists not found: ${_file}")
    ENDIF ()
ENDMACRO ()


# Append the given dependencies to the source file
MACRO (ADD_SOURCE_DEPENDENCIES _source _deps)
    SET(_tmp)
    GET_SOURCE_FILE_PROPERTY(_tmp ${_source} OBJECT_DEPENDS)
    IF (NOT _tmp)
        SET(_tmp "")
    ENDIF ()

    FOREACH (f ${_deps})
        LIST(APPEND _tmp "${f}")
    ENDFOREACH ()

    SET_SOURCE_FILES_PROPERTIES(${_source} PROPERTIES OBJECT_DEPENDS "${_tmp}")
ENDMACRO ()


MACRO (GENERATE_DOM_NAMES _namespace _attrs)
    SET(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    SET(_arguments  --attrs ${_attrs})
    SET(_outputfiles ${DERIVED_SOURCES_DIR}/${_namespace}Names.cpp ${DERIVED_SOURCES_DIR}/${_namespace}Names.h)
    SET(_extradef)
    SET(_tags)

    FOREACH (f ${ARGN})
        IF (_tags)
            SET(_extradef "${_extradef} ${f}")
        ELSE ()
            SET(_tags ${f})
        ENDIF ()
    ENDFOREACH ()

    IF (_tags)
        SET(_arguments "${_arguments}" --tags ${_tags} --factory --wrapperFactory)
        SET(_outputfiles "${_outputfiles}" ${DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.cpp ${DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.h ${DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.cpp ${DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.h)
    ENDIF ()

    IF (_extradef)
        SET(_additionArguments "${_additionArguments}" --extraDefines=${_extradef})
    ENDIF ()

    ADD_CUSTOM_COMMAND(
        OUTPUT  ${_outputfiles}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS} ${_attrs} ${_tags}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --outputDir ${DERIVED_SOURCES_DIR} ${_arguments}
        VERBATIM)
ENDMACRO ()


# - Create hash table *.lut.h
# GENERATE_HASH_LUT(input_file output_file)
MACRO (GENERATE_HASH_LUT _input _output)
    SET(HASH_LUT_GENERATOR "${JAVASCRIPTCORE_DIR}/create_hash_table")

    FOREACH (_tmp ${ARGN})
        IF (${_tmp} STREQUAL "MAIN_DEPENDENCY")
            SET(_main_dependency ${_input})
        ENDIF ()
    ENDFOREACH ()

    ADD_CUSTOM_COMMAND(
        OUTPUT ${_output}
        MAIN_DEPENDENCY ${_main_dependency}
        DEPENDS ${_input} ${HASH_LUT_GENERATOR}
        COMMAND ${PERL_EXECUTABLE} ${HASH_LUT_GENERATOR} ${_input} > ${_output}
        VERBATIM)
ENDMACRO ()


MACRO (GENERATE_GRAMMAR _prefix _input _output_header _output_source)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${_output_header} ${_output_source}
        MAIN_DEPENDENCY ${_input}
        COMMAND ${BISON_EXECUTABLE} -p ${_prefix} ${_input} -o ${_output_source} --defines=${_output_header}
        VERBATIM)
ENDMACRO ()

MACRO(MAKE_HASH_TOOLS _source)
    GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)

    IF (${_source} STREQUAL "DocTypeStrings")
        SET(_hash_tools_h "${DERIVED_SOURCES_DIR}/HashTools.h")
    ELSE ()
        SET(_hash_tools_h "")
    ENDIF ()

    ADD_CUSTOM_COMMAND(
        OUTPUT ${DERIVED_SOURCES_DIR}/${_name}.cpp ${_hash_tools_h}
        MAIN_DEPENDENCY ${_source}.gperf 
        COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/make-hash-tools.pl ${DERIVED_SOURCES_DIR} ${_source}.gperf
        VERBATIM)

    UNSET(_name)
    UNSET(_hash_tools_h)
ENDMACRO()

MACRO (WEBKIT_WRAP_SOURCELIST _input)
    IF (WTF_PLATFORM_QT)
        FOREACH (_file ${ARGN})
            GET_FILENAME_COMPONENT(_basename ${_file} NAME_WE)
            SET(_moc_filename ${DERIVED_SOURCES_DIR}/${_basename}.moc)

            FILE(READ ${_file} _contents)

            STRING(REGEX MATCHALL "#include[ ]+\"${_basename}\\.moc\"" _match "${_contents}")
            IF (_match)
                QT4_GENERATE_MOC(${_file} ${_moc_filename})
                ADD_SOURCE_DEPENDENCIES(${_file} ${_moc_filename})
            ENDIF ()
        ENDFOREACH ()
    ENDIF ()
ENDMACRO ()


MACRO (GENERATE_ENTITIES _input _output _script)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${_output}
        MAIN_DEPENDENCY ${_input} ${_script}
        COMMAND ${PYTHON_EXECUTABLE} ${_script} -o ${_output} ${_input}
        VERBATIM)
ENDMACRO ()
