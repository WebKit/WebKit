macro(WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS)
    set(_file ${CMAKE_CURRENT_SOURCE_DIR}/Platform${PORT}.cmake)
    if (EXISTS ${_file})
        message(STATUS "Using platform-specific CMakeLists: ${_file}")
        include(${_file})
    else ()
        message(STATUS "Platform-specific CMakeLists not found: ${_file}")
    endif ()
endmacro()

# Append the given dependencies to the source file
macro(ADD_SOURCE_DEPENDENCIES _source _deps)
    set(_tmp)
    get_source_file_property(_tmp ${_source} OBJECT_DEPENDS)
    if (NOT _tmp)
        set(_tmp "")
    endif ()

    foreach (f ${_deps})
        list(APPEND _tmp "${f}")
    endforeach ()

    set_source_files_properties(${_source} PROPERTIES OBJECT_DEPENDS "${_tmp}")
endmacro()

macro(ADD_PRECOMPILED_HEADER _header _cpp _source)
    if (MSVC)
        get_filename_component(PrecompiledBasename ${_header} NAME_WE)
        set(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${PrecompiledBasename}.pch")
        set(_sources ${${_source}})

        set_source_files_properties(${_cpp}
            PROPERTIES COMPILE_FLAGS "/Yc\"${_header}\" /Fp\"${PrecompiledBinary}\""
            OBJECT_OUTPUTS "${PrecompiledBinary}")
        set_source_files_properties(${_sources}
            PROPERTIES COMPILE_FLAGS "/Yu\"${_header}\" /FI\"${_header}\" /Fp\"${PrecompiledBinary}\""
            OBJECT_DEPENDS "${PrecompiledBinary}")
        list(APPEND ${_source} ${_cpp})
    endif ()
    #FIXME: Add support for Xcode.
endmacro()

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
macro(GENERATE_BINDINGS _output_source _input_files _base_dir _idl_includes _features _destination _prefix _generator _extension _idl_attributes_file)
    set(BINDING_GENERATOR ${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl)
    set(_args ${ARGN})
    list(LENGTH _args _argCount)
    if (_argCount GREATER 0)
        list(GET _args 0 _supplemental_dependency_file)
        if (_supplemental_dependency_file)
            set(_supplemental_dependency --supplementalDependencyFile ${_supplemental_dependency_file})
        endif ()
        list(GET _args 1 _additional_dependencies)
    endif ()

    set(COMMON_GENERATOR_DEPENDENCIES
        ${BINDING_GENERATOR}
        ${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm
        ${SCRIPTS_BINDINGS}
        ${_supplemental_dependency_file}
        ${_idl_attributes_file}
    )
    list(APPEND COMMON_GENERATOR_DEPENDENCIES ${_additional_dependencies})

    if (EXISTS ${WEBCORE_DIR}/bindings/scripts/CodeGenerator${_generator}.pm)
        list(APPEND COMMON_GENERATOR_DEPENDENCIES ${WEBCORE_DIR}/bindings/scripts/CodeGenerator${_generator}.pm)
    endif ()

    foreach (_file ${_input_files})
        get_filename_component(_name ${_file} NAME_WE)

        # Not all ObjC bindings generate a .mm file, and not all .mm files generated should be compiled.
        if (${_generator} STREQUAL "ObjC")
            list(FIND ObjC_BINDINGS_NO_MM ${_name} _no_mm_index)
            if (${_no_mm_index} EQUAL -1)
                set(_no_mm 0)
            else ()
                set(_no_mm 1)
            endif ()
        else ()
            set(_no_mm 0)
        endif ()

        if (${_no_mm})
            add_custom_command(
                OUTPUT ${_destination}/${_prefix}${_name}.h
                MAIN_DEPENDENCY ${_file}
                DEPENDS ${COMMON_GENERATOR_DEPENDENCIES}
                COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${BINDING_GENERATOR} --defines "${_features}" --generator ${_generator} ${_idl_includes} --outputDir "${_destination}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" --idlAttributesFile ${_idl_attributes_file} ${_supplemental_dependency} ${_file}
                WORKING_DIRECTORY ${_base_dir}
                VERBATIM)

            list(APPEND ${_output_source} ${_destination}/${_prefix}${_name}.h)
        else ()
            add_custom_command(
                OUTPUT ${_destination}/${_prefix}${_name}.${_extension} ${_destination}/${_prefix}${_name}.h
                MAIN_DEPENDENCY ${_file}
                DEPENDS ${COMMON_GENERATOR_DEPENDENCIES}
                COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${BINDING_GENERATOR} --defines "${_features}" --generator ${_generator} ${_idl_includes} --outputDir "${_destination}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" --idlAttributesFile ${_idl_attributes_file} ${_supplemental_dependency} ${_file}
                WORKING_DIRECTORY ${_base_dir}
                VERBATIM)
            list(APPEND ${_output_source} ${_destination}/${_prefix}${_name}.${_extension})
        endif ()
    endforeach ()
endmacro()

macro(GENERATE_FONT_NAMES _infile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    set(_arguments  --fonts ${_infile})
    set(_outputfiles ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitFontFamilyNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitFontFamilyNames.h)

    add_custom_command(
        OUTPUT  ${_outputfiles}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${MAKE_NAMES_DEPENDENCIES} ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR} ${_arguments}
        VERBATIM)
endmacro()


macro(GENERATE_EVENT_FACTORY _infile _outfile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_event_factory.pl)

    add_custom_command(
        OUTPUT  ${DERIVED_SOURCES_WEBCORE_DIR}/${_outfile}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --input ${_infile} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR}
        VERBATIM)
endmacro()


macro(GENERATE_EXCEPTION_CODE_DESCRIPTION _infile _outfile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_dom_exceptions.pl)

    add_custom_command(
        OUTPUT  ${DERIVED_SOURCES_WEBCORE_DIR}/${_outfile}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --input ${_infile} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR}
        VERBATIM)
endmacro()


macro(GENERATE_SETTINGS_MACROS _infile _outfile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/page/make_settings.pl)

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/${_outfile} ${DERIVED_SOURCES_WEBCORE_DIR}/InternalSettingsGenerated.h ${DERIVED_SOURCES_WEBCORE_DIR}/InternalSettingsGenerated.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/InternalSettingsGenerated.idl
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --input ${_infile} --outputDir ${DERIVED_SOURCES_WEBCORE_DIR}
        VERBATIM)
endmacro()


macro(GENERATE_DOM_NAMES _namespace _attrs)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    set(_arguments  --attrs ${_attrs})
    set(_outputfiles ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}Names.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}Names.h)
    set(_extradef)
    set(_tags)

    foreach (f ${ARGN})
        if (_tags)
            set(_extradef "${_extradef} ${f}")
        else ()
            set(_tags ${f})
        endif ()
    endforeach ()

    if (_tags)
        set(_arguments "${_arguments}" --tags ${_tags} --factory --wrapperFactory)
        set(_outputfiles "${_outputfiles}" ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}ElementFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/${_namespace}ElementFactory.h ${DERIVED_SOURCES_WEBCORE_DIR}/JS${_namespace}ElementWrapperFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/JS${_namespace}ElementWrapperFactory.h)
    endif ()

    if (_extradef)
        set(_additionArguments "${_additionArguments}" --extraDefines=${_extradef})
    endif ()

    add_custom_command(
        OUTPUT  ${_outputfiles}
        DEPENDS ${MAKE_NAMES_DEPENDENCIES} ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS} ${_attrs} ${_tags}
        COMMAND ${PERL_EXECUTABLE} -I${WEBCORE_DIR}/bindings/scripts ${NAMES_GENERATOR} --preprocessor "${CODE_GENERATOR_PREPROCESSOR_WITH_LINEMARKERS}" --outputDir ${DERIVED_SOURCES_WEBCORE_DIR} ${_arguments} ${_additionArguments}
        VERBATIM)
endmacro()


macro(GENERATE_GRAMMAR _prefix _input _output_header _output_source _features)
    # This is a workaround for winflexbison, which does not work corretly when
    # run in a different working directory than the installation directory.
    get_filename_component(_working_directory ${BISON_EXECUTABLE} PATH)

    add_custom_command(
        OUTPUT ${_output_header} ${_output_source}
        MAIN_DEPENDENCY ${_input}
        DEPENDS ${_input}
        COMMAND ${PERL_EXECUTABLE} -I ${WEBCORE_DIR}/bindings/scripts ${WEBCORE_DIR}/css/makegrammar.pl --outputDir ${DERIVED_SOURCES_WEBCORE_DIR} --extraDefines "${_features}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" --bison "${BISON_EXECUTABLE}" --symbolsPrefix ${_prefix} ${_input}
        WORKING_DIRECTORY ${_working_directory}
        VERBATIM)
endmacro()

macro(MAKE_HASH_TOOLS _source)
    get_filename_component(_name ${_source} NAME_WE)

    if (${_source} STREQUAL "DocTypeStrings")
        set(_hash_tools_h "${DERIVED_SOURCES_WEBCORE_DIR}/HashTools.h")
    else ()
        set(_hash_tools_h "")
    endif ()

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/${_name}.cpp ${_hash_tools_h}
        MAIN_DEPENDENCY ${_source}.gperf
        COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/make-hash-tools.pl ${DERIVED_SOURCES_WEBCORE_DIR} ${_source}.gperf ${GPERF_EXECUTABLE}
        VERBATIM)

    unset(_name)
    unset(_hash_tools_h)
endmacro()

macro(WEBKIT_WRAP_SOURCELIST)
    foreach (_file ${ARGN})
        get_filename_component(_basename ${_file} NAME_WE)
        get_filename_component(_path ${_file} PATH)

        if (NOT _file MATCHES "${DERIVED_SOURCES_WEBCORE_DIR}")
            string(REGEX REPLACE "/" "\\\\\\\\" _sourcegroup "${_path}")
            source_group("${_sourcegroup}" FILES ${_file})
        endif ()
    endforeach ()

    source_group("DerivedSources" REGULAR_EXPRESSION "${DERIVED_SOURCES_WEBCORE_DIR}")
endmacro()

macro(WEBKIT_FRAMEWORK _target)
    include_directories(${${_target}_INCLUDE_DIRECTORIES})
    include_directories(SYSTEM ${${_target}_SYSTEM_INCLUDE_DIRECTORIES})
    add_library(${_target} ${${_target}_LIBRARY_TYPE}
        ${${_target}_HEADERS}
        ${${_target}_SOURCES}
        ${${_target}_DERIVED_SOURCES}
    )
    target_link_libraries(${_target} ${${_target}_LIBRARIES})
    set_target_properties(${_target} PROPERTIES COMPILE_DEFINITIONS "BUILDING_${_target}")
    set_target_properties(${_target} PROPERTIES FOLDER "${_target}")

    if (${_target}_OUTPUT_NAME)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${${_target}_OUTPUT_NAME})
    endif ()

    if (${_target}_PRE_BUILD_COMMAND)
        add_custom_target(_${_target}_PreBuild COMMAND ${${_target}_PRE_BUILD_COMMAND} VERBATIM)
        add_dependencies(${_target} _${_target}_PreBuild)
    endif ()

    if (${_target}_POST_BUILD_COMMAND)
        add_custom_command(TARGET ${_target} POST_BUILD COMMAND ${${_target}_POST_BUILD_COMMAND} VERBATIM)
    endif ()

    if (APPLE AND NOT PORT STREQUAL "GTK")
        set_target_properties(${_target} PROPERTIES FRAMEWORK TRUE)
        install(TARGETS ${_target} FRAMEWORK DESTINATION ${LIB_INSTALL_DIR})
    endif ()
endmacro()

macro(WEBKIT_CREATE_FORWARDING_HEADER _target_directory _file)
    get_filename_component(_source_path "${CMAKE_SOURCE_DIR}/Source/" ABSOLUTE)
    get_filename_component(_absolute "${_file}" ABSOLUTE)
    get_filename_component(_name "${_file}" NAME)
    set(_target_filename "${_target_directory}/${_name}")

    # Try to make the path in the forwarding header relative to the Source directory
    # so that these forwarding headers are compatible with the ones created by the
    # WebKit2 generate-forwarding-headers script.
    string(REGEX REPLACE "${_source_path}/" "" _relative ${_absolute})

    set(_content "#include \"${_relative}\"\n")

    if (EXISTS "${_target_filename}")
        file(READ "${_target_filename}" _old_content)
    endif ()

    if (NOT _old_content STREQUAL _content)
        file(WRITE "${_target_filename}" "${_content}")
    endif ()
endmacro()

macro(WEBKIT_CREATE_FORWARDING_HEADERS _framework)
    # On Windows, we copy the entire contents of forwarding headers.
    if (NOT WIN32)
        set(_processing_directories 0)
        set(_processing_files 0)
        set(_target_directory "${DERIVED_SOURCES_DIR}/ForwardingHeaders/${_framework}")

        file(GLOB _files "${_target_directory}/*.h")
        foreach (_file ${_files})
            file(READ "${_file}" _content)
            string(REGEX MATCH "^#include \"([^\"]*)\"" _matched ${_content})
            if (_matched AND NOT EXISTS "${CMAKE_SOURCE_DIR}/Source/${CMAKE_MATCH_1}")
               file(REMOVE "${_file}")
            endif ()
        endforeach ()

        foreach (_currentArg ${ARGN})
            if ("${_currentArg}" STREQUAL "DIRECTORIES")
                set(_processing_directories 1)
                set(_processing_files 0)
            elseif ("${_currentArg}" STREQUAL "FILES")
                set(_processing_directories 0)
                set(_processing_files 1)
            elseif (_processing_directories)
                file(GLOB _files "${_currentArg}/*.h")
                foreach (_file ${_files})
                    WEBKIT_CREATE_FORWARDING_HEADER(${_target_directory} ${_file})
                endforeach ()
            elseif (_processing_files)
                WEBKIT_CREATE_FORWARDING_HEADER(${_target_directory} ${_currentArg})
            endif ()
        endforeach ()
    endif ()
endmacro()

# Helper macro which wraps generate-message-receiver.py and generate-message-header.py scripts
#   _output_source is a list name which will contain generated sources.(eg. WebKit2_SOURCES)
#   _input_files are messages.in files to generate.
macro(GENERATE_WEBKIT2_MESSAGE_SOURCES _output_source _input_files)
    foreach (_file ${_input_files})
        get_filename_component(_name ${_file} NAME_WE)
        add_custom_command(
            OUTPUT ${DERIVED_SOURCES_WEBKIT2_DIR}/${_name}MessageReceiver.cpp ${DERIVED_SOURCES_WEBKIT2_DIR}/${_name}Messages.h
            MAIN_DEPENDENCY ${_file}
            DEPENDS ${WEBKIT2_DIR}/Scripts/webkit/__init__.py
                    ${WEBKIT2_DIR}/Scripts/webkit/messages.py
                    ${WEBKIT2_DIR}/Scripts/webkit/model.py
                    ${WEBKIT2_DIR}/Scripts/webkit/parser.py
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-message-receiver.py ${_file} > ${DERIVED_SOURCES_WEBKIT2_DIR}/${_name}MessageReceiver.cpp
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-messages-header.py ${_file} > ${DERIVED_SOURCES_WEBKIT2_DIR}/${_name}Messages.h
            WORKING_DIRECTORY ${WEBKIT2_DIR}
            VERBATIM)

        list(APPEND ${_output_source} ${DERIVED_SOURCES_WEBKIT2_DIR}/${_name}MessageReceiver.cpp)
    endforeach ()
endmacro()

# Helper macro for using all-in-one builds
# This macro removes the sources included in the _all_in_one_file from the input _file_list.
# _file_list is a list of source files
# _all_in_one_file is an all-in-one cpp file includes other cpp files
# _result_file_list is the output file list
macro(PROCESS_ALLINONE_FILE _file_list _all_in_one_file _result_file_list)
    file(STRINGS ${_all_in_one_file} _all_in_one_file_content)
    set(${_result_file_list} ${_file_list})
    foreach (_line ${_all_in_one_file_content})
        string(REGEX MATCH "^#include [\"<](.*)[\">]" _found ${_line})
        if (_found)
            list(APPEND _allins ${CMAKE_MATCH_1})
        endif ()
    endforeach ()

    foreach (_allin ${_allins})
        string(REGEX REPLACE ";[^;]*/${_allin};" ";" _new_result "${${_result_file_list}};")
        set(${_result_file_list} ${_new_result})
    endforeach ()

endmacro()
