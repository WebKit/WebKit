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


# Helper macro which wraps generate-bindings.pl script.
#   _output_source is a list name which will contain generated sources.(eg. WebCore_SOURCES)
#   _input_files are IDL files to generate.
#   _base_dir is base directory where script is called.
#   _idl_includes is value of --include argument. (eg. --include=${WEBCORE_DIR}/bindings/js)
#   _features is a value of --defines argument.
#   _destination is a value of --outputDir argument.
#   _prefix is a prefix of output files. (eg. JS - it makes JSXXX.cpp JSXXX.h from XXX.idl)
#   _generator is a value of --generator argument.
#   _supplemental_dependency_file is a value of --supplementalDependencyFile. (optional)
MACRO (GENERATE_BINDINGS _output_source _input_files _base_dir _idl_includes _features _destination _prefix _generator)
    SET(BINDING_GENERATOR ${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl)

    SET(_supplemental_dependency_file ${ARGN})
    IF (_supplemental_dependency_file)
        SET(_supplemental_dependency --supplementalDependencyFile ${_supplemental_dependency_file})
    ENDIF ()

    FOREACH (_file ${_input_files})
        GET_FILENAME_COMPONENT (_name ${_file} NAME_WE)

        ADD_CUSTOM_COMMAND(
            OUTPUT ${_destination}/${_prefix}${_name}.cpp ${_destination}/${_prefix}${_name}.h
            MAIN_DEPENDENCY ${_file}
            DEPENDS ${BINDING_GENERATOR} ${SCRIPTS_BINDINGS} ${_supplemental_dependency_file}
            COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${BINDING_GENERATOR} --defines "${_features}" --generator ${_generator} ${_idl_includes} --outputDir "${_destination}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${_supplemental_dependency} ${_file}
            WORKING_DIRECTORY ${_base_dir}
            VERBATIM)

        LIST(APPEND ${_output_source} ${_destination}/${_prefix}${_name}.cpp)
    ENDFOREACH ()
ENDMACRO ()


MACRO (GENERATE_FONT_NAMES _infile)
    SET(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    SET(_arguments  --fonts ${_infile})
    SET(_outputfiles ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitFontFamilyNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitFontFamilyNames.h)

    ADD_CUSTOM_COMMAND(
        OUTPUT  ${_outputfiles}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR} ${_arguments}
        VERBATIM)
ENDMACRO ()


MACRO (GENERATE_EVENT_FACTORY _infile _outfile)
    SET(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_event_factory.pl)

    ADD_CUSTOM_COMMAND(
        OUTPUT  ${DERIVED_SOURCES_WEBCORE_DIR}/${_outfile}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --input ${_infile} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR}
        VERBATIM)
ENDMACRO ()


MACRO (GENERATE_EXCEPTION_CODE_DESCRIPTION _infile _outfile)
    SET(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_dom_exceptions.pl)

    ADD_CUSTOM_COMMAND(
        OUTPUT  ${DERIVED_SOURCES_WEBCORE_DIR}/${_outfile}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --input ${_infile} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR}
        VERBATIM)
ENDMACRO ()


MACRO (GENERATE_DOM_NAMES _namespace _attrs)
    SET(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    SET(_arguments  --attrs ${_attrs})
    SET(_outputfiles ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}Names.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}Names.h)
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
        SET(_outputfiles "${_outputfiles}" ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}ElementFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}ElementFactory.h ${DERIVED_SOURCES_WEBCORE_DIR}/JS${_namespace}ElementWrapperFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/JS${_namespace}ElementWrapperFactory.h)
    ENDIF ()

    IF (_extradef)
        SET(_additionArguments "${_additionArguments}" --extraDefines=${_extradef})
    ENDIF ()

    ADD_CUSTOM_COMMAND(
        OUTPUT  ${_outputfiles}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS} ${_attrs} ${_tags}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --preprocessor "${CODE_GENERATOR_PREPROCESSOR_WITH_LINEMARKERS}" --outputDir ${DERIVED_SOURCES_WEBCORE_DIR} ${_arguments} ${_additionArguments}
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
        COMMAND ${PERL_EXECUTABLE} ${HASH_LUT_GENERATOR} ${_input} -i > ${_output}
        VERBATIM)
ENDMACRO ()


MACRO (GENERATE_GRAMMAR _prefix _input _output_header _output_source _other_dependencies _features)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${_output_header} ${_output_source}
        MAIN_DEPENDENCY ${_input}
        DEPENDS ${_input} ${_other_dependencies}
        COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/makegrammar.pl --grammar ${_input} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR} --extraDefines "${_features}" --symbolsPrefix ${_prefix}
        VERBATIM)
ENDMACRO ()

MACRO(MAKE_HASH_TOOLS _source)
    GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)

    IF (${_source} STREQUAL "DocTypeStrings")
        SET(_hash_tools_h "${DERIVED_SOURCES_WEBCORE_DIR}/HashTools.h")
    ELSE ()
        SET(_hash_tools_h "")
    ENDIF ()

    ADD_CUSTOM_COMMAND(
        OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/${_name}.cpp ${_hash_tools_h}
        MAIN_DEPENDENCY ${_source}.gperf 
        COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/make-hash-tools.pl ${DERIVED_SOURCES_WEBCORE_DIR} ${_source}.gperf
        VERBATIM)

    UNSET(_name)
    UNSET(_hash_tools_h)
ENDMACRO()

MACRO (WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS)
    INCLUDE_IF_EXISTS(${CMAKE_CURRENT_SOURCE_DIR}/Platform${PORT}.cmake)
ENDMACRO ()

MACRO (WEBKIT_WRAP_SOURCELIST)
    FOREACH (_file ${ARGN})
        GET_FILENAME_COMPONENT(_basename ${_file} NAME_WE)
        GET_FILENAME_COMPONENT(_path ${_file} PATH)

        IF (NOT _file MATCHES "${DERIVED_SOURCES_WEBCORE_DIR}")
            STRING(REGEX REPLACE "/" "\\\\\\\\" _sourcegroup "${_path}")
            SOURCE_GROUP("${_sourcegroup}" FILES ${_file})
        ENDIF ()

        IF (WTF_PLATFORM_QT)
            SET(_moc_filename ${DERIVED_SOURCES_WEBCORE_DIR}/${_basename}.moc)

            FILE(READ ${_file} _contents)

            STRING(REGEX MATCHALL "#include[ ]+\"${_basename}\\.moc\"" _match "${_contents}")
            IF (_match)
                QT4_GENERATE_MOC(${_file} ${_moc_filename})
                ADD_SOURCE_DEPENDENCIES(${_file} ${_moc_filename})
            ENDIF ()
        ENDIF ()
    ENDFOREACH ()

    SOURCE_GROUP("DerivedSources" REGULAR_EXPRESSION "${DERIVED_SOURCES_WEBCORE_DIR}")
ENDMACRO ()


MACRO (WEBKIT_CREATE_FORWARDING_HEADER _target_directory _file)
    GET_FILENAME_COMPONENT(_absolute "${_file}" ABSOLUTE)
    GET_FILENAME_COMPONENT(_name "${_file}" NAME)
    SET(_content "#include \"${_absolute}\"\n")
    SET(_filename "${_target_directory}/${_name}")

    IF (EXISTS "${_filename}")
        FILE(READ "${_filename}" _old_content)
    ENDIF ()

    IF (NOT _old_content STREQUAL _content)
        FILE(WRITE "${_filename}" "${_content}")
    ENDIF ()
ENDMACRO ()

MACRO (WEBKIT_CREATE_FORWARDING_HEADERS _framework)
    SET(_processing_directories 0)
    SET(_processing_files 0)
    SET(_target_directory "${DERIVED_SOURCES_DIR}/ForwardingHeaders/${_framework}")

    FILE(GLOB _files "${_target_directory}/*.h")
    FOREACH (_file ${_files})
        FILE(READ "${_file}" _content)
        STRING(REGEX MATCH "^#include \"([^\"]*)\"" _matched ${_content})
        IF (_matched AND NOT EXISTS "${CMAKE_MATCH_1}")
           FILE(REMOVE "${_file}")
        ENDIF()
    ENDFOREACH ()

    FOREACH (_currentArg ${ARGN})
        IF ("${_currentArg}" STREQUAL "DIRECTORIES")
            SET(_processing_directories 1)
            SET(_processing_files 0)
        ELSEIF ("${_currentArg}" STREQUAL "FILES")
            SET(_processing_directories 0)
            SET(_processing_files 1)
        ELSEIF (_processing_directories)
            FILE(GLOB _files "${_currentArg}/*.h")
            FOREACH (_file ${_files})
                WEBKIT_CREATE_FORWARDING_HEADER(${_target_directory} ${_file})
            ENDFOREACH ()
        ELSEIF (_processing_files)
            WEBKIT_CREATE_FORWARDING_HEADER(${_target_directory} ${_currentArg})
        ENDIF ()
    ENDFOREACH ()
ENDMACRO ()
