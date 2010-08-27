# This file provides various generators used by webkit.
# It will check for the programs and define the given executables:
#    PERL_EXECUTABLE
#    BISON_EXECUTABLE
#    GPERF_EXECUTABLE
#    FLEX_EXECUTABLE

INCLUDE (WebKitFS)

# Modules that the bindings generator scripts may use
SET(SCRIPTS_BINDINGS
  ${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm
  ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
  ${WEBCORE_DIR}/bindings/scripts/IDLStructure.pm
  ${WEBCORE_DIR}/bindings/scripts/InFilesParser.pm)
SET(BINDING_CODE_GENERATOR ${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl)
SET(JS_IDL_FILES "")
SET(Inspector_IDL_FILES "")

# - Create JS C++ code given an IDL input
# GENERATE_FROM_IDL(generator idl_source)
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
    DEPENDS ${BINDING_CODE_GENERATOR} ${SCRIPTS_BINDINGS} ${WEBCORE_DIR}/${_source}
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${BINDING_CODE_GENERATOR} ${IDL_INCLUDES} --outputDir "${DERIVED_SOURCES_DIR}" --defines "LANGUAGE_JAVASCRIPT=1 ${FEATURE_DEFINES_STR}" --generator JS ${WEBCORE_DIR}/${_source}
    VERBATIM)
  LIST(APPEND JS_IDL_FILES ${DERIVED_SOURCES_DIR}/JS${_name}.cpp)
  UNSET(_name)
  UNSET(_defines)
ENDMACRO()


# - Create Inspector C++ code given an IDL input
# GENERATE_FROM_IDL(generator idl_source)
#
# The generated files (.cpp, .h) lives in ${DERIVED_SOURCES_DIR}.
#
# This function also appends the generated cpp file to Inspector_IDL_FILES list.
MACRO(GENERATE_INSPECTOR_FROM_IDL _source)
  SET(FEATURE_DEFINES_STR "")
  FOREACH (f ${FEATURE_DEFINES})
    SET(FEATURE_DEFINES_STR "${FEATURE_DEFINES_STR} ${f}")
  ENDFOREACH ()

  GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${DERIVED_SOURCES_DIR}/${_name}Frontend.cpp ${DERIVED_SOURCES_DIR}/${_name}Frontend.h ${DERIVED_SOURCES_DIR}/${_name}BackendDispatcher.cpp ${DERIVED_SOURCES_DIR}/${_name}BackendDispatcher.h
    DEPENDS ${BINDING_CODE_GENERATOR} ${SCRIPTS_BINDINGS} ${WEBCORE_DIR}/${_source}
    COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts -I${WEBCORE_DIR}/inspector ${BINDING_CODE_GENERATOR} ${IDL_INCLUDES} --outputDir "${DERIVED_SOURCES_DIR}" --defines "LANGUAGE_JAVASCRIPT=1 ${FEATURE_DEFINES_STR}" --generator Inspector ${WEBCORE_DIR}/${_source}
    VERBATIM)
  LIST(APPEND Inspector_IDL_FILES ${DERIVED_SOURCES_DIR}/${_name}Frontend.cpp)
  UNSET(_name)
  UNSET(_defines)
ENDMACRO()



# - Create pure JavaScript functions (does nothing so far)
MACRO(GENERATE_JS_FROM_IDL_PURE _source)
   GET_FILENAME_COMPONENT(_name ${_source} NAME_WE)
   ADD_CUSTOM_COMMAND(
     OUTPUT  ${DERIVED_SOURCES_DIR}/JS${_name}.cpp ${DERIVED_SOURCES_DIR}/JS${_name}.h
     DEPENDS ${BINDING_CODE_GENERATOR} ${SCRIPTS_BINDINGS} ${WEBCORE_DIR}/${_source}
     COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${BINDING_CODE_GENERATOR} ${IDL_INCLUDES} --outputDir "${DERIVED_SOURCES_DIR}" --defines "LANGUAGE_JAVASCRIPT=1 ${FEATURE_DEFINES_STR}" --generator JS ${WEBCORE_DIR}/${_source}
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
  ${WEBCORE_DIR}/css/mediaControls.css)
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
    OUTPUT ${DERIVED_SOURCES_DIR}/CSSValueKeywords.h ${DERIVED_SOURCES_DIR}/CSSValueKeywords.cpp ${DERIVED_SOURCES_DIR}/CSSValueKeywords.in ${DERIVED_SOURCES_DIR}/CSSValueKeywords.gperf
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
