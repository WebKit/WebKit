# This file is for macros that are used by multiple projects. If your macro is
# exclusively needed in only one subdirectory of Source (e.g. only needed by
# WebCore), then put it there instead.

macro(WEBKIT_COMPUTE_SOURCES _framework)
    foreach (_sourcesListFile IN LISTS ${_framework}_UNIFIED_SOURCE_LIST_FILES)
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}" "${DERIVED_SOURCES_DIR}/${_framework}/${_sourcesListFile}" COPYONLY)
      message(STATUS "Using source list file: ${_sourcesListFile}")

      list(APPEND _sourceListFileTruePaths "${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}")
    endforeach ()

    if (WIN32 AND INTERNAL_BUILD)
        set(WTF_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/WTF/Scripts")
    else ()
        set(WTF_SCRIPTS_DIR "${FORWARDING_HEADERS_DIR}/wtf/Scripts")
    endif ()

    if (ENABLE_UNIFIED_BUILDS)
        execute_process(COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.rb
            "--derived-sources-path" "${DERIVED_SOURCES_DIR}/${_framework}"
            "--source-tree-path" ${CMAKE_CURRENT_SOURCE_DIR}
            "--print-bundled-sources"
            "--feature-flags" "${UNIFIED_SOURCE_LIST_ENABLED_FEATURES}"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
             message(FATAL_ERROR "generate-unified-source-bundles.rb exited with non-zero status, exiting")
        endif ()

        foreach (_sourceFileTmp IN LISTS _outputTmp)
            set_source_files_properties(${_sourceFileTmp} PROPERTIES HEADER_FILE_ONLY ON)
            list(APPEND ${_framework}_HEADERS ${_sourceFileTmp})
        endforeach ()
        unset(_sourceFileTmp)

        execute_process(COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.rb
            "--derived-sources-path" "${DERIVED_SOURCES_DIR}/${_framework}"
            "--source-tree-path" ${CMAKE_CURRENT_SOURCE_DIR}
            "--feature-flags" "${UNIFIED_SOURCE_LIST_ENABLED_FEATURES}"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE  _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
            message(FATAL_ERROR "generate-unified-source-bundles.rb exited with non-zero status, exiting")
        endif ()

        list(APPEND ${_framework}_SOURCES ${_outputTmp})
        unset(_resultTmp)
        unset(_outputTmp)
    else ()
        execute_process(COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.rb
            "--derived-sources-path" "${DERIVED_SOURCES_DIR}/${_framework}"
            "--source-tree-path" ${CMAKE_CURRENT_SOURCE_DIR}
            "--print-all-sources"
            "--feature-flags" "${UNIFIED_SOURCE_LIST_ENABLED_FEATURES}"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
             message(FATAL_ERROR "generate-unified-source-bundles.rb exited with non-zero status, exiting")
        endif ()

        list(APPEND ${_framework}_SOURCES ${_outputTmp})
        unset(_resultTmp)
        unset(_outputTmp)
    endif ()
endmacro()

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
macro(WEBKIT_ADD_SOURCE_DEPENDENCIES _source _deps)
    set(_tmp)
    get_source_file_property(_tmp ${_source} OBJECT_DEPENDS)
    if (NOT _tmp)
        set(_tmp "")
    endif ()

    foreach (f ${_deps})
        list(APPEND _tmp "${f}")
    endforeach ()

    set_source_files_properties(${_source} PROPERTIES OBJECT_DEPENDS "${_tmp}")
    unset(_tmp)
endmacro()

macro(WEBKIT_ADD_PRECOMPILED_HEADER _header _cpp _source)
    if (MSVC)
        get_filename_component(PrecompiledBasename ${_cpp} NAME_WE)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${_source}")
        set(PrecompiledBinary "${CMAKE_CURRENT_BINARY_DIR}/${_source}/${PrecompiledBasename}.pch")
        set(_sources ${${_source}})

        # clang-cl requires /FI with /Yc
        if (COMPILER_IS_CLANG_CL)
            set_source_files_properties(${_cpp}
                PROPERTIES COMPILE_FLAGS "/Yc\"${_header}\" /Fp\"${PrecompiledBinary}\" /FI\"${_header}\""
                OBJECT_OUTPUTS "${PrecompiledBinary}")
        else ()
            set_source_files_properties(${_cpp}
                PROPERTIES COMPILE_FLAGS "/Yc\"${_header}\" /Fp\"${PrecompiledBinary}\""
                OBJECT_OUTPUTS "${PrecompiledBinary}")
        endif ()
        set_source_files_properties(${_sources}
            PROPERTIES COMPILE_FLAGS "/Yu\"${_header}\" /FI\"${_header}\" /Fp\"${PrecompiledBinary}\"")

        foreach (_src ${_sources})
            WEBKIT_ADD_SOURCE_DEPENDENCIES(${_src} ${PrecompiledBinary})
        endforeach ()

        list(APPEND ${_source} ${_cpp})
    endif ()
    #FIXME: Add support for Xcode.
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

macro(WEBKIT_FRAMEWORK_DECLARE _target)
    # add_library() without any source files triggers CMake warning
    # Addition of dummy "source" file does not result in any changes in generated build.ninja file
    add_library(${_target} ${${_target}_LIBRARY_TYPE} "${CMAKE_BINARY_DIR}/cmakeconfig.h")
endmacro()

macro(WEBKIT_FRAMEWORK _target)
    target_sources(${_target} PRIVATE
        ${${_target}_HEADERS}
        ${${_target}_SOURCES}
    )
    target_include_directories(${_target} PUBLIC "$<BUILD_INTERFACE:${${_target}_INCLUDE_DIRECTORIES}>")
    target_include_directories(${_target} SYSTEM PRIVATE "$<BUILD_INTERFACE:${${_target}_SYSTEM_INCLUDE_DIRECTORIES}>")
    target_include_directories(${_target} PRIVATE "$<BUILD_INTERFACE:${${_target}_PRIVATE_INCLUDE_DIRECTORIES}>")
    target_link_libraries(${_target} ${${_target}_LIBRARIES})
    set_target_properties(${_target} PROPERTIES COMPILE_DEFINITIONS "BUILDING_${_target}")

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

    if (APPLE AND NOT PORT STREQUAL "GTK" AND NOT ${${_target}_LIBRARY_TYPE} MATCHES STATIC)
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
        set(_target_directory "${FORWARDING_HEADERS_DIR}/${_framework}")

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

function(WEBKIT_MAKE_FORWARDING_HEADERS framework)
    set(options FLATTENED)
    set(oneValueArgs DESTINATION TARGET_NAME)
    set(multiValueArgs DIRECTORIES EXTRA_DIRECTORIES DERIVED_SOURCE_DIRECTORIES FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(headers ${opt_FILES})
    if (opt_DESTINATION)
        set(destination ${opt_DESTINATION})
    else ()
        set(destination ${FORWARDING_HEADERS_DIR}/${framework})
    endif ()
    file(MAKE_DIRECTORY ${destination})
    foreach (dir IN LISTS opt_DIRECTORIES)
        file(GLOB files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${dir}/*.h)
        list(APPEND headers ${files})
    endforeach ()
    set(fwd_headers)
    foreach (header IN LISTS headers)
        if (opt_FLATTENED)
            get_filename_component(header_filename ${header} NAME)
            set(fwd_header ${destination}/${header_filename})
        else ()
            get_filename_component(header_dir ${header} DIRECTORY)
            file(MAKE_DIRECTORY ${destination}/${header_dir})
            set(fwd_header ${destination}/${header})
        endif ()
        add_custom_command(OUTPUT ${fwd_header}
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${header} ${fwd_header}
            MAIN_DEPENDENCY ${header}
            VERBATIM
        )
        list(APPEND fwd_headers ${fwd_header})
    endforeach ()
    foreach (dir IN LISTS opt_EXTRA_DIRECTORIES)
        set(dir ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
        file(GLOB_RECURSE files RELATIVE ${dir} ${dir}/*.h)
        foreach (header IN LISTS files)
            get_filename_component(header_dir ${header} DIRECTORY)
            file(MAKE_DIRECTORY ${destination}/${header_dir})
            set(fwd_header ${destination}/${header})
            add_custom_command(OUTPUT ${fwd_header}
                COMMAND ${CMAKE_COMMAND} -E copy ${dir}/${header} ${fwd_header}
                MAIN_DEPENDENCY ${dir}/${header}
                VERBATIM
            )
            list(APPEND fwd_headers ${fwd_header})
        endforeach ()
    endforeach ()
    if (opt_TARGET_NAME)
        set(target_name ${opt_TARGET_NAME})
    else ()
        set(target_name ${framework}ForwardingHeaders)
    endif ()
    add_custom_target(${target_name} DEPENDS ${fwd_headers})
    add_dependencies(${framework} ${target_name})
    if (opt_DERIVED_SOURCE_DIRECTORIES)
        set(script ${CMAKE_CURRENT_BINARY_DIR}/makeForwardingHeaders.cmake)
        set(content "")
        foreach (dir IN LISTS opt_DERIVED_SOURCE_DIRECTORIES)
            string(CONCAT content ${content}
                "file(GLOB headers \"${dir}/*.h\")\n"
                "foreach (header IN LISTS headers)\n"
                "    get_filename_component(header_filename \${header} NAME)\n"
                "    execute_process(COMMAND \${CMAKE_COMMAND} -E copy_if_different \${header} ${destination}/\${header_filename} RESULT_VARIABLE result)\n"
                "    if (NOT \${result} EQUAL 0)\n"
                "        message(FATAL_ERROR \"Failed to copy \${header}: \${result}\")\n"
                "    endif ()\n"
                "endforeach ()\n"
            )
        endforeach ()
        file(WRITE ${script} ${content})
        add_custom_command(TARGET ${framework} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P ${script}
            VERBATIM
        )
    endif ()
endfunction()

# Helper macros for debugging CMake problems.
macro(WEBKIT_DEBUG_DUMP_COMMANDS)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endmacro()

macro(WEBKIT_DEBUG_DUMP_VARIABLES)
    set_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
       message(STATUS "${_variableName}=${${_variableName}}")
    endforeach ()
endmacro()

# Append the given flag to the target property.
# Builds on top of get_target_property() and set_target_properties()
macro(WEBKIT_ADD_TARGET_PROPERTIES _target _property _flags)
    get_target_property(_tmp ${_target} ${_property})
    if (NOT _tmp)
        set(_tmp "")
    endif (NOT _tmp)

    foreach (f ${_flags})
        set(_tmp "${_tmp} ${f}")
    endforeach (f ${_flags})

    set_target_properties(${_target} PROPERTIES ${_property} ${_tmp})
    unset(_tmp)
endmacro()

macro(WEBKIT_POPULATE_LIBRARY_VERSION library_name)
    if (NOT DEFINED ${library_name}_VERSION_MAJOR)
        set(${library_name}_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    endif ()
    if (NOT DEFINED ${library_name}_VERSION_MINOR)
        set(${library_name}_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    endif ()
    if (NOT DEFINED ${library_name}_VERSION_MICRO)
        set(${library_name}_VERSION_MICRO ${PROJECT_VERSION_MICRO})
    endif ()
    if (NOT DEFINED ${library_name}_VERSION)
        set(${library_name}_VERSION ${PROJECT_VERSION})
    endif ()
endmacro()

macro(WEBKIT_CREATE_SYMLINK target src dest)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ln -sf ${src} ${dest}
        DEPENDS ${dest}
        COMMENT "Create symlink from ${src} to ${dest}")
endmacro()
