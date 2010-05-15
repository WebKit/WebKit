# This file provides various generators used by webkit.
# It will check for the programs and define the given executables:
#    PERL_EXECUTABLE
#    BISON_EXECUTABLE
#    GPERF_EXECUTABLE
#    FLEX_EXECUTABLE

INCLUDE (WebKitFS)

FIND_PACKAGE(Perl REQUIRED)

# - Create hash table *.lut.h
# GENERATE_HASH_LUT(input_file output_file)
MACRO(GENERATE_HASH_LUT _input _output)
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
ENDMACRO()

# - Create hash table *.lut.h using at JavaScriptCore/runtime
# GENERATE_HASH_LUT_RUNTIME(source)
#
# The generated files lives in ${CMAKE_BINARY_DIR}/JavaScriptCore/runtime/
# and will have suffix ".lut.h"
#
# Input file is assumed to be in JavaScriptCore/runtime/${source}.cpp
MACRO(GENERATE_HASH_LUT_RUNTIME _file)
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${CMAKE_BINARY_DIR}/JavaScriptCore/runtime/${_file}.lut.h
    DEPENDS ${JAVASCRIPTCORE_DIR}/runtime/${_file}.cpp  ${HASH_LUT_GENERATOR}
    COMMAND ${PERL_EXECUTABLE} ${HASH_LUT_GENERATOR} ${JAVASCRIPTCORE_DIR}/runtime/${_file}.cpp -i > ${CMAKE_BINARY_DIR}/JavaScriptCore/runtime/${_file}.lut.h
    VERBATIM)
  LIST(APPEND GENERATED_HASH_LUT_RUNTIME_FILES "${CMAKE_BINARY_DIR}/JavaScriptCore/runtime/${_file}.lut.h")
ENDMACRO()


FIND_PROGRAM (BISON_EXECUTABLE bison)
IF (NOT BISON_EXECUTABLE)
  MESSAGE(FATAL_ERROR "Missing bison")
ENDIF (NOT BISON_EXECUTABLE)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Bison DEFAULT_MSG BISON_EXECUTABLE)

# - Create a grammar using bison.
# GENERATE_GRAMMAR(prefix source_file)
#
# Reads a source_file (*.y) Generates the .cpp and .h files in
# ${DERIVED_SOURCES_DIR}
MACRO(GENERATE_GRAMMAR _prefix _source)
  GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)
  SET(_out_base ${DERIVED_SOURCES_DIR}/${_name})
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${_out_base}.cpp ${_out_base}.h
    DEPENDS ${_source}
    COMMAND ${BISON_EXECUTABLE} -p ${_prefix} ${_source} -o ${_out_base}.cpp --defines=${_out_base}.h
    VERBATIM)
  UNSET(_out_base)
  UNSET(_name)
ENDMACRO ()


FIND_PROGRAM(GPERF_EXECUTABLE gperf)
IF (NOT GPERF_EXECUTABLE)
  MESSAGE(FATAL_ERROR "Missing gperf")
ENDIF ()
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GPerf DEFAULT_MSG GPERF_EXECUTABLE)

# - Create perfect hash tables using gperf
# GENERATE_GPERF(extension source_file find_function gperf_options)
#
# The generated files lives in ${DERIVED_SOURCES_DIR} and ends in the
# given extension.
MACRO(GENERATE_GPERF _ext _source _func _opts)
  GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_DIR}/${_name}.${_ext}
    DEPENDS ${_source}
    COMMAND ${GPERF_EXECUTABLE} -CDEGIot -L ANSI-C -k * -s 2 -N ${_func} ${_opts} --output-file=${DERIVED_SOURCES_DIR}/${_name}.${_ext} ${_source}
    VERBATIM)
ENDMACRO ()


# Modules that the bindings generator scripts may use
SET(SCRIPTS_BINDINGS
  ${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm
  ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
  ${WEBCORE_DIR}/bindings/scripts/IDLStructure.pm
  ${WEBCORE_DIR}/bindings/scripts/InFilesParser.pm)
SET(JS_CODE_GENERATOR ${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl)
SET(JS_IDL_FILES "")
# - Create JavaScript C++ code given an IDL input
# GENERATE_JS_FROM_IDL(idl_source)
#
# The generated files (.cpp, .h) lives in ${DERIVED_SOURCES_DIR}.
#
# This function also appends the generated cpp file to JS_IDL_FILES list.
MACRO(GENERATE_JS_FROM_IDL _source)
  SET(FEATURE_DEFINES_STR "")
  FOREACH (f ${FEATURE_DEFINES})
    SET(FEATURE_DEFINES_STR "${FEATURE_DEFINES_STR} ${f}")
  ENDFOREACH ()

  GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${DERIVED_SOURCES_DIR}/JS${_name}.cpp ${DERIVED_SOURCES_DIR}/JS${_name}.h
    DEPENDS ${JS_CODE_GENERATOR} ${SCRIPTS_BINDINGS} ${WEBCORE_DIR}/${_source}
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${JS_CODE_GENERATOR} ${IDL_INCLUDES} --outputDir "${DERIVED_SOURCES_DIR}" --defines "LANGUAGE_JAVASCRIPT=1 ${FEATURE_DEFINES_STR}" --generator JS ${WEBCORE_DIR}/${_source}
    VERBATIM)
  LIST(APPEND JS_IDL_FILES ${DERIVED_SOURCES_DIR}/JS${_name}.cpp)
  UNSET(_name)
  UNSET(_defines)
ENDMACRO()

# - Create pure JavaScript functions (does nothing so far)
MACRO(GENERATE_JS_FROM_IDL_PURE _source)
   GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)
   ADD_CUSTOM_COMMAND(
     OUTPUT  ${DERIVED_SOURCES_DIR}/JS${_name}.cpp ${DERIVED_SOURCES_DIR}/JS${_name}.h
     DEPENDS ${JS_CODE_GENERATOR} ${SCRIPTS_BINDINGS} ${WEBCORE_DIR}/${_source}
     COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${JS_CODE_GENERATOR} ${IDL_INCLUDES} --outputDir "${DERIVED_SOURCES_DIR}" --defines "LANGUAGE_JAVASCRIPT=1 ${FEATURE_DEFINES_STR}" --generator JS ${WEBCORE_DIR}/${_source}
     VERBATIM)
   UNSET(_name)
ENDMACRO()

SET(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
# - Create DOM names and factory given tags and attributes as source.
# GENERATE_DOM_NAMES_FACTORY(namespace tags_source attributes_source [defines])
#
# The generated files lives in ${DERIVED_SOURCES_DIR}. The files will
# be named using the given namespace, such as:
#   - ${namespace}Names.cpp, ${namespace}Names.h;
#   - ${namespace}ElementFactory.cpp, ${namespace}ElementFactory.h;
#   - ${namespace}ElementWrapperFactory.cpp; ${namespace}ElementWrapperFactory.h
#
# If optional defines are given, then they will be speficied using
# --extraDefines directive to the generator script.
MACRO(GENERATE_DOM_NAMES_FACTORY _namespace _tags _attrs)
  UNSET(_extradef)
  FOREACH (f ${ARGN})
    SET(_extradef "${_extradef} ${f}")
  ENDFOREACH ()
  IF (_extradef)
    SET(_extradef --extraDefines=${_extradef})
  ENDIF ()
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_DIR}/${_namespace}Names.cpp ${DERIVED_SOURCES_DIR}/${_namespace}Names.h ${DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.cpp ${DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.h ${DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.cpp ${DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.h
    DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS} ${_tags} ${_attrs}
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --tags ${_tags} --attrs ${_attrs} ${_extradef} --factory --wrapperFactory --outputDir ${DERIVED_SOURCES_DIR}
    VERBATIM)
  UNSET(_extradef)
ENDMACRO ()

# - Create DOM names only (no factories)
# GENERATE_DOM_NAMES_ONLY(namespace attributes_source)
#
# The generated files lives in ${DERIVED_SOURCES_DIR}. The files will
# be named using the given namespace, such as:
#   - ${namespace}Names.cpp, ${namespace}Names.h;
MACRO(GENERATE_DOM_NAMES_ONLY _namespace _attrs)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_DIR}/${_namespace}Names.cpp ${DERIVED_SOURCES_DIR}/${_namespace}Names.h
    DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS} ${_attrs}
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --attrs ${_attrs} --outputDir ${DERIVED_SOURCES_DIR}
    VERBATIM)
ENDMACRO()

# - Create ${CMAKE_BINARY_DIR}/JavaScriptCore/pcre/chartables.c
# GENERATE_DFTABLES()
MACRO(GENERATE_DFTABLES)
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${CMAKE_BINARY_DIR}/JavaScriptCore/pcre/chartables.c
    DEPENDS ${JAVASCRIPTCORE_DIR}/pcre/dftables
    COMMAND ${PERL_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/pcre/dftables ${CMAKE_BINARY_DIR}/JavaScriptCore/pcre/chartables.c
    VERBATIM)
ENDMACRO()


FIND_PROGRAM(FLEX_EXECUTABLE flex)
IF (NOT FLEX_EXECUTABLE)
  MESSAGE(FATAL_ERROR "Missing flex")
ENDIF ()
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Flex DEFAULT_MSG FLEX_EXECUTABLE)

SET(MAKE_TOKENIZER ${WEBCORE_DIR}/css/maketokenizer)
# - Create ${DERIVED_SOURCES_DIR}/tokenizer.cpp
# GENERATE_TOKENIZER()
MACRO(GENERATE_TOKENIZER)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_DIR}/tokenizer.cpp
    DEPENDS ${WEBCORE_DIR}/css/tokenizer.flex ${MAKE_TOKENIZER}
    COMMAND ${FLEX_EXECUTABLE} -t ${WEBCORE_DIR}/css/tokenizer.flex | ${PERL_EXECUTABLE} ${MAKE_TOKENIZER} > ${DERIVED_SOURCES_DIR}/tokenizer.cpp
    VERBATIM)
ENDMACRO()


SET(USER_AGENT_STYLE_SHEETS
  ${WEBCORE_DIR}/css/html.css
  ${WEBCORE_DIR}/css/mathml.css
  ${WEBCORE_DIR}/css/quirks.css
  ${WEBCORE_DIR}/css/view-source.css
  ${WEBCORE_DIR}/css/svg.css
  ${WEBCORE_DIR}/css/wml.css
  ${WEBCORE_DIR}/css/mediaControls.css
  ${WEBCORE_DIR}/css/mediaControlsGtk.css)
SET(USER_AGENT_STYLE_SHEETS_GENERATOR ${WEBCORE_DIR}/css/make-css-file-arrays.pl)
# - Create ${DERIVED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp and
#   ${DERIVED_SOURCES_DIR}/UserAgentStyleSheets.h
# GENERATE_USER_AGENT_STYLES()
MACRO(GENERATE_USER_AGENT_STYLES)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp ${DERIVED_SOURCES_DIR}/UserAgentStyleSheets.h
    DEPENDS ${USER_AGENT_STYLE_SHEETS_GENERATOR} ${USER_AGENT_STYLE_SHEETS}
    COMMAND ${PERL_EXECUTABLE} ${USER_AGENT_STYLE_SHEETS_GENERATOR} ${DERIVED_SOURCES_DIR}/UserAgentStyleSheets.h ${DERIVED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp ${USER_AGENT_STYLE_SHEETS}
    VERBATIM)
ENDMACRO ()


SET(CSS_VALUE_KEYWORDS
  ${WEBCORE_DIR}/css/CSSValueKeywords.in
  ${WEBCORE_DIR}/css/SVGCSSValueKeywords.in)
SET(CSS_VALUE_GENERATOR ${WEBCORE_DIR}/css/makevalues.pl)
# - Create ${DERIVED_SOURCES_DIR}/CSSValueKeywords.*
# GENERATE_CSS_VALUE_KEYWORDS()
MACRO(GENERATE_CSS_VALUE_KEYWORDS)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_DIR}/CSSValueKeywords.h ${DERIVED_SOURCES_DIR}/CSSValueKeywords.c ${DERIVED_SOURCES_DIR}/CSSValueKeywords.in ${DERIVED_SOURCES_DIR}/CSSValueKeywords.gperf
    DEPENDS ${CSS_VALUE_KEYWORDS} ${CSS_VALUE_GENERATOR}
    WORKING_DIRECTORY ${DERIVED_SOURCES_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print lc" ${CSS_VALUE_KEYWORDS} > ${DERIVED_SOURCES_DIR}/CSSValueKeywords.in
    COMMAND ${PERL_EXECUTABLE} ${CSS_VALUE_GENERATOR}
    VERBATIM)
ENDMACRO ()

SET(CSS_PROPERTY_NAMES
  ${WEBCORE_DIR}/css/CSSPropertyNames.in
  ${WEBCORE_DIR}/css/SVGCSSPropertyNames.in)
SET(MAKE_CSS_PROP ${WEBCORE_DIR}/css/makeprop.pl)
# - Create ${DERIVED_SOURCES_DIR}/CSSPropertyNames.*
# GENERATE_CSS_PROPERTY_NAMES()
MACRO(GENERATE_CSS_PROPERTY_NAMES)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_DIR}/CSSPropertyNames.h ${DERIVED_SOURCES_DIR}/CSSPropertyNames.cpp ${DERIVED_SOURCES_DIR}/CSSPropertyNames.in ${DERIVED_SOURCES_DIR}/CSSPropertyNames.gperf
    DEPENDS ${MAKE_CSS_PROP} ${CSS_PROPERTY_NAMES}
    WORKING_DIRECTORY ${DERIVED_SOURCES_DIR}
    COMMAND cat ${CSS_PROPERTY_NAMES} > ${DERIVED_SOURCES_DIR}/CSSPropertyNames.in
    COMMAND ${PERL_EXECUTABLE} ${MAKE_CSS_PROP}
    VERBATIM)
ENDMACRO ()
