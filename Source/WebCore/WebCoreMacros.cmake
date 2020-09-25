macro(MAKE_HASH_TOOLS _source)
    get_filename_component(_name ${_source} NAME_WE)

    if (${_source} STREQUAL "DocTypeStrings")
        set(_hash_tools_h "${WebCore_DERIVED_SOURCES_DIR}/HashTools.h")
    else ()
        set(_hash_tools_h "")
    endif ()

    add_custom_command(
        OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/${_name}.cpp ${_hash_tools_h}
        MAIN_DEPENDENCY ${_source}.gperf
        COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/make-hash-tools.pl ${WebCore_DERIVED_SOURCES_DIR} ${_source}.gperf ${GPERF_EXECUTABLE}
        VERBATIM)

    unset(_name)
    unset(_hash_tools_h)
endmacro()


# Append the given dependencies to the source file
# This one consider the given dependencies are in ${WebCore_DERIVED_SOURCES_DIR}
# and prepends this to every member of dependencies list
macro(ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES _source _deps)
    set(_tmp "")
    foreach (f ${_deps})
        list(APPEND _tmp "${WebCore_DERIVED_SOURCES_DIR}/${f}")
    endforeach ()

    WEBKIT_ADD_SOURCE_DEPENDENCIES(${_source} ${_tmp})
    unset(_tmp)
endmacro()


macro(MAKE_JS_FILE_ARRAYS _output_cpp _output_h _namespace _scripts _scripts_dependencies)
    add_custom_command(
        OUTPUT ${_output_h} ${_output_cpp}
        DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/make-js-file-arrays.py ${${_scripts}}
        COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/make-js-file-arrays.py -n ${_namespace} ${_output_h} ${_output_cpp} ${${_scripts}}
        VERBATIM)
    WEBKIT_ADD_SOURCE_DEPENDENCIES(${${_scripts_dependencies}} ${_output_h} ${_output_cpp})
endmacro()


option(SHOW_BINDINGS_GENERATION_PROGRESS "Show progress of generating bindings" OFF)

# Helper macro which wraps generate-bindings-all.pl script.
#   target is a new target name to be added
#   OUTPUT_SOURCE is a list name which will contain generated sources.(eg. WebCore_SOURCES)
#   INPUT_FILES are IDL files to generate.
#   PP_INPUT_FILES are IDL files to preprocess.
#   BASE_DIR is base directory where script is called.
#   IDL_INCLUDES is value of --include argument. (eg. ${WEBCORE_DIR}/bindings/js)
#   FEATURES is a value of --defines argument.
#   DESTINATION is a value of --outputDir argument.
#   GENERATOR is a value of --generator argument.
#   SUPPLEMENTAL_DEPFILE is a value of --supplementalDependencyFile. (optional)
#   PP_EXTRA_OUTPUT is extra outputs of preprocess-idls.pl. (optional)
#   PP_EXTRA_ARGS is extra arguments for preprocess-idls.pl. (optional)
function(GENERATE_BINDINGS target)
    set(options)
    set(oneValueArgs OUTPUT_SOURCE BASE_DIR FEATURES DESTINATION GENERATOR SUPPLEMENTAL_DEPFILE)
    set(multiValueArgs INPUT_FILES PP_INPUT_FILES IDL_INCLUDES PP_EXTRA_OUTPUT PP_EXTRA_ARGS)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(binding_generator ${WEBCORE_DIR}/bindings/scripts/generate-bindings-all.pl)
    set(idl_attributes_file ${WEBCORE_DIR}/bindings/scripts/IDLAttributes.json)
    set(idl_files_list ${CMAKE_CURRENT_BINARY_DIR}/idl_files_${target}.tmp)
    set(pp_idl_files_list ${CMAKE_CURRENT_BINARY_DIR}/pp_idl_files_${target}.tmp)
    set(_supplemental_dependency)

    set(content)
    foreach (f ${arg_INPUT_FILES})
        if (NOT IS_ABSOLUTE ${f})
            set(f ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        endif ()
        set(content "${content}${f}\n")
    endforeach ()
    file(WRITE ${idl_files_list} ${content})

    set(pp_content)
    foreach (f ${arg_PP_INPUT_FILES})
        if (NOT IS_ABSOLUTE ${f})
            set(f ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        endif ()
        set(pp_content "${pp_content}${f}\n")
    endforeach ()
    file(WRITE ${pp_idl_files_list} ${pp_content})

    set(args
        --defines ${arg_FEATURES}
        --generator ${arg_GENERATOR}
        --outputDir ${arg_DESTINATION}
        --idlFilesList ${idl_files_list}
        --ppIDLFilesList ${pp_idl_files_list}
        --preprocessor "${CODE_GENERATOR_PREPROCESSOR}"
        --idlAttributesFile ${idl_attributes_file}
    )
    if (arg_SUPPLEMENTAL_DEPFILE)
        list(APPEND args --supplementalDependencyFile ${arg_SUPPLEMENTAL_DEPFILE})
    endif ()
    ProcessorCount(PROCESSOR_COUNT)
    if (PROCESSOR_COUNT)
        list(APPEND args --numOfJobs ${PROCESSOR_COUNT})
    endif ()
    foreach (i IN LISTS arg_IDL_INCLUDES)
        if (IS_ABSOLUTE ${i})
            list(APPEND args --include ${i})
        else ()
            list(APPEND args --include ${CMAKE_CURRENT_SOURCE_DIR}/${i})
        endif ()
    endforeach ()
    foreach (i IN LISTS arg_PP_EXTRA_OUTPUT)
        list(APPEND args --ppExtraOutput ${i})
    endforeach ()
    foreach (i IN LISTS arg_PP_EXTRA_ARGS)
        list(APPEND args --ppExtraArgs ${i})
    endforeach ()

    set(common_generator_dependencies
        ${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl
        ${SCRIPTS_BINDINGS}
        # Changing enabled features should trigger recompiling all IDL files
        # because some of them use #if.
        ${CMAKE_BINARY_DIR}/cmakeconfig.h
    )
    if (EXISTS ${WEBCORE_DIR}/bindings/scripts/CodeGenerator${arg_GENERATOR}.pm)
        list(APPEND common_generator_dependencies ${WEBCORE_DIR}/bindings/scripts/CodeGenerator${arg_GENERATOR}.pm)
    endif ()
    if (EXISTS ${arg_BASE_DIR}/CodeGenerator${arg_GENERATOR}.pm)
        list(APPEND common_generator_dependencies ${arg_BASE_DIR}/CodeGenerator${arg_GENERATOR}.pm)
    endif ()
    foreach (i IN LISTS common_generator_dependencies)
        list(APPEND args --generatorDependency ${i})
    endforeach ()

    set(gen_sources)
    set(gen_headers)
    foreach (_file ${arg_INPUT_FILES})
        get_filename_component(_name ${_file} NAME_WE)
        list(APPEND gen_sources ${arg_DESTINATION}/JS${_name}.cpp)
        list(APPEND gen_headers ${arg_DESTINATION}/JS${_name}.h)
    endforeach ()
    set(${arg_OUTPUT_SOURCE} ${${arg_OUTPUT_SOURCE}} ${gen_sources} PARENT_SCOPE)
    set(act_args)
    if (SHOW_BINDINGS_GENERATION_PROGRESS)
        list(APPEND args --showProgress)
    endif ()
    list(APPEND act_args BYPRODUCTS ${gen_sources} ${gen_headers})
    if (SHOW_BINDINGS_GENERATION_PROGRESS)
        list(APPEND act_args USES_TERMINAL)
    endif ()
    add_custom_target(${target}
        COMMAND ${PERL_EXECUTABLE} ${binding_generator} ${args}
        DEPENDS ${arg_INPUT_FILES} ${arg_PP_INPUT_FILES}
        WORKING_DIRECTORY ${arg_BASE_DIR}
        COMMENT "Generate bindings (${target})"
        VERBATIM ${act_args})
endfunction()


macro(GENERATE_FONT_NAMES _infile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    set(_arguments  --fonts ${_infile})
    set(_outputfiles ${WebCore_DERIVED_SOURCES_DIR}/WebKitFontFamilyNames.cpp ${WebCore_DERIVED_SOURCES_DIR}/WebKitFontFamilyNames.h)

    add_custom_command(
        OUTPUT  ${_outputfiles}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${MAKE_NAMES_DEPENDENCIES} ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} ${NAMES_GENERATOR} --outputDir ${WebCore_DERIVED_SOURCES_DIR} ${_arguments}
        VERBATIM)
endmacro()


macro(GENERATE_EVENT_FACTORY _infile _namespace)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_event_factory.pl)
    set(_outputfiles ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Interfaces.h ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Factory.cpp)

    add_custom_command(
        OUTPUT  ${_outputfiles}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} ${NAMES_GENERATOR} --input ${_infile} --outputDir ${WebCore_DERIVED_SOURCES_DIR}
        VERBATIM)
endmacro()


macro(GENERATE_SETTINGS_MACROS _infile _outfile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/Scripts/GenerateSettings.rb)

    set(_extra_output
        ${WebCore_DERIVED_SOURCES_DIR}/Settings.cpp
        ${WebCore_DERIVED_SOURCES_DIR}/InternalSettingsGenerated.h
        ${WebCore_DERIVED_SOURCES_DIR}/InternalSettingsGenerated.cpp
        ${WebCore_DERIVED_SOURCES_DIR}/InternalSettingsGenerated.idl
    )

    set(GENERATE_SETTINGS_SCRIPTS
        ${WEBCORE_DIR}/Scripts/SettingsTemplates/InternalSettingsGenerated.cpp.erb
        ${WEBCORE_DIR}/Scripts/SettingsTemplates/InternalSettingsGenerated.idl.erb
        ${WEBCORE_DIR}/Scripts/SettingsTemplates/InternalSettingsGenerated.h.erb
        ${WEBCORE_DIR}/Scripts/SettingsTemplates/Settings.cpp.erb
        ${WEBCORE_DIR}/Scripts/SettingsTemplates/Settings.h.erb
    )

    add_custom_command(
        OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/${_outfile} ${_extra_output}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${GENERATE_SETTINGS_SCRIPTS} ${SCRIPTS_BINDINGS}
        COMMAND ${RUBY_EXECUTABLE} ${NAMES_GENERATOR} --input ${_infile} --outputDir ${WebCore_DERIVED_SOURCES_DIR}
        VERBATIM ${_args})
endmacro()


function(GENERATE_DOM_NAMES _namespace _attrs)
    list(POP_FRONT ARGN _tags)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    set(_arguments  --attrs ${_attrs})
    set(_outputfiles ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Names.cpp ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Names.h)

    if (_tags)
        set(_arguments "${_arguments}" --tags ${_tags} --factory --wrapperFactory)
        set(_outputfiles "${_outputfiles}" ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.cpp ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.h ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}ElementTypeHelpers.h ${WebCore_DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.cpp ${WebCore_DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.h)
    endif ()

    add_custom_command(
        OUTPUT  ${_outputfiles}
        DEPENDS ${MAKE_NAMES_DEPENDENCIES} ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS} ${_attrs} ${_tags}
        COMMAND ${PERL_EXECUTABLE} ${NAMES_GENERATOR} --outputDir ${WebCore_DERIVED_SOURCES_DIR} ${_arguments} ${_additionArguments}
        VERBATIM)
endfunction()
