# This file is for macros that are used by multiple projects. If your macro is
# exclusively needed in only one subdirectory of Source (e.g. only needed by
# WebCore), then put it there instead.

macro(WEBKIT_COMPUTE_SOURCES _framework)
    set(_derivedSourcesPath ${${_framework}_DERIVED_SOURCES_DIR})

    foreach (_sourcesListFile IN LISTS ${_framework}_UNIFIED_SOURCE_LIST_FILES)
      if (${_framework}_UNIFIED_SOURCE_EXCLUDES)
          file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}" _allLines)
          set(_filtered "")
          foreach (_line IN LISTS _allLines)
              set(_skip FALSE)
              foreach (_pattern IN LISTS ${_framework}_UNIFIED_SOURCE_EXCLUDES)
                  if (_line MATCHES "${_pattern}")
                      set(_skip TRUE)
                      break ()
                  endif ()
              endforeach ()
              if (NOT _skip)
                  string(APPEND _filtered "${_line}\n")
              endif ()
          endforeach ()
          file(WRITE "${_derivedSourcesPath}/${_sourcesListFile}" "${_filtered}")
      else ()
          configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}" "${_derivedSourcesPath}/${_sourcesListFile}" COPYONLY)
      endif ()
      message(STATUS "Using source list file: ${_sourcesListFile}")

      list(APPEND _sourceListFileTruePaths "${_derivedSourcesPath}/${_sourcesListFile}")
    endforeach ()

    set(gusb_args --derived-sources-path ${_derivedSourcesPath} --source-tree-path ${CMAKE_CURRENT_SOURCE_DIR})
    # Windows needs a larger bundle size because that helps keep WebCore.lib's size below the 4GB maximum in debug builds.
    if (MSVC AND ${_framework} STREQUAL "WebCore" AND ${_framework}_LIBRARY_TYPE STREQUAL "STATIC")
        list(APPEND gusb_args --max-bundle-size 16)
    endif ()
    if (${_framework} STREQUAL "WebCore")
        list(APPEND gusb_args --dense-bundle-filter "JS*=JSBindings" --dense-bundle-filter "bindings/*=JSBindings")
    endif ()

    if (ENABLE_UNIFIED_BUILDS)
        execute_process(COMMAND ${Python_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.py
            ${gusb_args}
            "--print-bundled-sources"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
             message(FATAL_ERROR "generate-unified-source-bundles.py exited with non-zero status, exiting")
        endif ()

        foreach (_sourceFileTmp IN LISTS _outputTmp)
            set_source_files_properties(${_sourceFileTmp} PROPERTIES HEADER_FILE_ONLY ON)
            list(APPEND ${_framework}_HEADERS ${_sourceFileTmp})
        endforeach ()
        unset(_sourceFileTmp)

        execute_process(COMMAND ${Python_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.py
            ${gusb_args}
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE  _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
            message(FATAL_ERROR "generate-unified-source-bundles.py exited with non-zero status, exiting")
        endif ()

        foreach (_file IN LISTS _outputTmp)
            if (_file MATCHES "\\.c$")
                list(APPEND ${_framework}_C_SOURCES ${_file})
            elseif (_file MATCHES "-ARC\\.mm$")
                # generate-unified-source-bundles.rb emits *-ARC.mm and *-nonARC.mm bundles based
                # on @nonARC annotations in Sources*.txt. The ARC bundles compile in a separate
                # OBJECT library so the OBJCXX precompiled header agrees on -fobjc-arc.
                list(APPEND ${_framework}_ARC_SOURCES ${_file})
            else ()
                list(APPEND ${_framework}_SOURCES ${_file})
            endif ()
        endforeach ()

        unset(_resultTmp)
        unset(_outputTmp)
    else ()
        execute_process(COMMAND ${Python_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.py
            ${gusb_args}
            "--print-all-sources"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
             message(FATAL_ERROR "generate-unified-source-bundles.py exited with non-zero status, exiting")
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

# Wrapper around target_precompile_headers().
#
# Swift sources are unaffected: with CMP0157 NEW (set in the top-level
# CMakeLists.txt) the Swift link rule receives only object files, so the
# .pch is never passed to swiftc.
#
# Targets that mix ARC and non-ARC .mm split the ARC sources into a separate
# OBJECT library (see ${_framework}_ARC_SOURCES) so each gets a matching PCH.
#
# On ports where OBJC/OBJCXX are not enabled languages those clauses are no-ops.
macro(WEBKIT_ADD_PREFIX_HEADER _target _header)
    target_precompile_headers(${_target} PRIVATE
        "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:${CMAKE_CURRENT_SOURCE_DIR}/${_header}>")
endmacro()

macro(_WEBKIT_PCH_PATHS_FOR_LANGUAGE _lang _out_src_ext _out_stub_ext _out_pch_stem)
    if (${_lang} STREQUAL "OBJCXX")
        set(${_out_src_ext} "mm")
        set(${_out_stub_ext} "mm")
        set(${_out_pch_stem} "cmake_pch.objcxx.hxx")
    elseif (${_lang} STREQUAL "OBJC")
        set(${_out_src_ext} "m")
        set(${_out_stub_ext} "m")
        set(${_out_pch_stem} "cmake_pch.objc.h")
    elseif (${_lang} STREQUAL "C")
        set(${_out_src_ext} "c")
        set(${_out_stub_ext} "c")
        set(${_out_pch_stem} "cmake_pch.h")
    else ()
        set(${_out_src_ext} "cpp")
        set(${_out_stub_ext} "cxx")
        set(${_out_pch_stem} "cmake_pch.hxx")
    endif ()
endmacro()

macro(WEBKIT_ADD_PREFIX_HEADER_WITH_PARENT _target _base_target _header _parent_header)
    if (COMPILER_IS_CLANG)
        string(JOIN "," _lang_genex ${ARGN})
        target_precompile_headers(${_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:${_lang_genex}>:${CMAKE_CURRENT_SOURCE_DIR}/${_header}>")
        get_target_property(_base_bin_dir ${_base_target} BINARY_DIR)
        get_target_property(_chain_bin_dir ${_target} BINARY_DIR)
        foreach (_lang ${ARGN})
            _WEBKIT_PCH_PATHS_FOR_LANGUAGE(${_lang} _src_ext _stub_ext _pch_stem)
            set(_base_pch "${_base_bin_dir}/CMakeFiles/${_base_target}.dir/${_pch_stem}.pch")
            set(_chain_stub "${_chain_bin_dir}/CMakeFiles/${_target}.dir/${_pch_stem}.${_stub_ext}")
            set_source_files_properties(${_chain_stub} PROPERTIES
                COMPILE_OPTIONS "-Xclang;-include-pch;-Xclang;${_base_pch}"
                OBJECT_DEPENDS "${_base_pch}")
        endforeach ()
    else ()
        WEBKIT_ADD_PREFIX_HEADER(${_target} ${_parent_header})
    endif ()
endmacro()

function(WEBKIT_DEFINE_SUBTARGET _target _parent)
    add_library(${_target} OBJECT)
    target_sources(${_target} PRIVATE ${${_parent}_HEADERS} ${ARGN})
    target_include_directories(${_target} PRIVATE $<TARGET_PROPERTY:${_parent},INCLUDE_DIRECTORIES>)
    target_include_directories(${_target} SYSTEM PRIVATE ${${_parent}_SYSTEM_INCLUDE_DIRECTORIES})
    target_compile_definitions(${_target} PRIVATE ${_parent}_EXPORTS $<TARGET_PROPERTY:${_parent},COMPILE_DEFINITIONS>)
    target_compile_options(${_target} PRIVATE $<TARGET_PROPERTY:${_parent},COMPILE_OPTIONS>)
    target_link_libraries(${_target} PRIVATE $<TARGET_PROPERTY:${_parent},LINK_LIBRARIES>)
    if (${_parent}_DEPENDENCIES)
        add_dependencies(${_target} ${${_parent}_DEPENDENCIES})
    endif ()
endfunction()

macro(WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX _target _subtarget)
    cmake_parse_arguments(_arg "" "PREFIX" "PREFIX_LANGUAGES;DIRS" ${ARGN})
    if (${_target}_FINALIZED)
        message(FATAL_ERROR "WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX(${_target} ${_subtarget}) must be called before WEBKIT_FRAMEWORK(${_target})")
    endif ()
    if (COMPILER_IS_CLANG AND NOT MSVC AND NOT CMAKE_DISABLE_PRECOMPILE_HEADERS)
        set(_src_exts)
        foreach (_lang IN LISTS _arg_PREFIX_LANGUAGES)
            _WEBKIT_PCH_PATHS_FOR_LANGUAGE(${_lang} _src_ext _stub_ext _pch_stem)
            list(APPEND _src_exts ${_src_ext})
        endforeach ()
        string(JOIN "|" _src_exts ${_src_exts})
        string(JOIN "|" _dirs ${_arg_DIRS})
        set(_re "(^|[-/])(${_dirs})[-/]")
        set(${_subtarget}_SOURCES ${${_target}_SOURCES})
        list(FILTER ${_subtarget}_SOURCES INCLUDE REGEX "${_re}")
        list(FILTER ${_subtarget}_SOURCES INCLUDE REGEX "\\.(${_src_exts})$")
        if (${_subtarget}_SOURCES)
            list(REMOVE_ITEM ${_target}_SOURCES ${${_subtarget}_SOURCES})
            WEBKIT_DEFINE_SUBTARGET(${_subtarget} ${_target} ${${_subtarget}_SOURCES})
            set(_subobjects "$<FILTER:$<TARGET_OBJECTS:${_subtarget}>,EXCLUDE,\\.(g|p)ch$>")
            if (${_target}_LIBRARY_TYPE STREQUAL "SHARED" OR ${_target}_LIBRARY_TYPE STREQUAL "MODULE")
                set(_rsp "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${_subtarget}.dir/$<CONFIG>/objects.rsp")
                file(GENERATE OUTPUT "${_rsp}" CONTENT "$<JOIN:${_subobjects},\n>")
                target_link_options(${_target} PRIVATE "@${_rsp}")
                set_property(TARGET ${_target} APPEND PROPERTY LINK_DEPENDS "${_subobjects};${_rsp}")
            else ()
                target_link_libraries(${_target} INTERFACE "${_subobjects}")
            endif ()
            WEBKIT_ADD_PREFIX_HEADER_WITH_PARENT(${_subtarget} ${_target} ${_arg_PREFIX} "" ${_arg_PREFIX_LANGUAGES})
        endif ()
    endif ()
    unset(_arg_PREFIX)
    unset(_arg_PREFIX_LANGUAGES)
    unset(_arg_DIRS)
endmacro()

macro(WEBKIT_FRAMEWORK_DECLARE _target)
    # add_library() without any source files triggers CMake warning
    # Addition of dummy "source" file does not result in any changes in generated build.ninja file
    add_library(${_target} ${${_target}_LIBRARY_TYPE} "${CMAKE_BINARY_DIR}/cmakeconfig.h")
endmacro()

macro(WEBKIT_LIBRARY_DECLARE _target)
    # add_library() without any source files triggers CMake warning
    # Addition of dummy "source" file does not result in any changes in generated build.ninja file
    add_library(${_target} ${${_target}_LIBRARY_TYPE} "${CMAKE_BINARY_DIR}/cmakeconfig.h")

    if (${_target}_LIBRARY_TYPE STREQUAL "OBJECT")
        list(APPEND ${_target}_INTERFACE_LIBRARIES "$<FILTER:$<TARGET_OBJECTS:${_target}>,EXCLUDE,\\.(g|p)ch$>")
        if (TARGET ${_target}_c)
            list(APPEND ${_target}_INTERFACE_LIBRARIES "$<FILTER:$<TARGET_OBJECTS:${_target}_c>,EXCLUDE,\\.(g|p)ch$>")
        endif ()
    endif ()
endmacro()

macro(WEBKIT_EXECUTABLE_DECLARE _target)
    add_executable(${_target} "${CMAKE_BINARY_DIR}/cmakeconfig.h")
endmacro()

# Private macro for setting the properties of a target.
macro(_WEBKIT_TARGET_SETUP _target _logical_name)
    if (USE_HEADER_MAPS AND ${_logical_name}_PRIVATE_INCLUDE_DIRECTORIES)
        WEBKIT_MAKE_HEADER_MAP(${_target} "${CMAKE_CURRENT_SOURCE_DIR}" ${_logical_name}_PRIVATE_INCLUDE_DIRECTORIES)
    endif ()
    target_include_directories(${_target} PUBLIC "$<BUILD_INTERFACE:${${_logical_name}_INCLUDE_DIRECTORIES}>")
    target_include_directories(${_target} SYSTEM PRIVATE "$<BUILD_INTERFACE:${${_logical_name}_SYSTEM_INCLUDE_DIRECTORIES}>")
    target_include_directories(${_target} PRIVATE "$<BUILD_INTERFACE:${${_logical_name}_PRIVATE_INCLUDE_DIRECTORIES}>")

    if (DEVELOPER_MODE_CXX_FLAGS)
        target_compile_options(${_target} PRIVATE
            "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:${DEVELOPER_MODE_CXX_FLAGS}>")
        target_compile_options(${_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Werror ExistentialAny -Werror StrictMemorySafety -Werror ForeignReferenceType>")
    endif ()

    target_compile_definitions(${_target} PRIVATE "BUILDING_${_logical_name}")
    if (${_logical_name}_DEFINITIONS)
        target_compile_definitions(${_target} PUBLIC ${${_logical_name}_DEFINITIONS})
    endif ()
    if (${_logical_name}_PRIVATE_DEFINITIONS)
        target_compile_definitions(${_target} PRIVATE ${${_logical_name}_PRIVATE_DEFINITIONS})
    endif ()

    if (${_logical_name}_COMPILE_OPTIONS)
        target_compile_options(${_target} PRIVATE ${${_logical_name}_COMPILE_OPTIONS})
    endif ()

    if (${_logical_name}_LIBRARIES)
        target_link_libraries(${_target} PUBLIC ${${_logical_name}_LIBRARIES})
    endif ()
    if (${_logical_name}_PRIVATE_LIBRARIES)
        target_link_libraries(${_target} PRIVATE ${${_logical_name}_PRIVATE_LIBRARIES})
    endif ()

    if (${_logical_name}_DEPENDENCIES)
        add_dependencies(${_target} ${${_logical_name}_DEPENDENCIES})
    endif ()
endmacro()

macro(_WEBKIT_TARGET _target)
    set(${_target}_FINALIZED TRUE)
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        if (${_target}_C_SOURCES)
            add_library(${_target}_c OBJECT)
            target_sources(${_target}_c PRIVATE ${${_target}_C_SOURCES})

            _WEBKIT_TARGET_SETUP(${_target}_c ${_target})

            set_target_properties(${_target}_c PROPERTIES C_STANDARD 17)
            list(APPEND ${_target}_PRIVATE_LIBRARIES ${_target}_c)
        endif ()

        target_sources(${_target} PRIVATE
            ${${_target}_HEADERS}
            ${${_target}_SOURCES}
        )

        _WEBKIT_TARGET_SETUP(${_target} ${_target})
    else ()
        target_sources(${_target} PRIVATE
            ${${_target}_HEADERS}
            ${${_target}_SOURCES}
            ${${_target}_C_SOURCES}
        )

        _WEBKIT_TARGET_SETUP(${_target} ${_target})
    endif ()
endmacro()

macro(_WEBKIT_TARGET_ANALYZE _target)
    if (ClangTidy_EXE)
        set(_clang_path_and_options
            ${ClangTidy_EXE}
            # Include all non system headers
            --header-filter=.*
        )
        set_target_properties(${_target} PROPERTIES
            C_CLANG_TIDY "${_clang_path_and_options}"
            CXX_CLANG_TIDY "${_clang_path_and_options}"
        )
    endif ()

    if (IWYU_EXE)
        set(_iwyu_path_and_options
            ${IWYU_EXE}
            # Suggests the more concise syntax introduced in C++17
            -Xiwyu --cxx17ns
            # Tells iwyu to always keep these includes
            -Xiwyu --keep=**/config.h
        )
        if (MSVC)
            list(APPEND _iwyu_path_and_options --driver-mode=cl)
        endif ()
        set_target_properties(${_target} PROPERTIES
            CXX_INCLUDE_WHAT_YOU_USE "${_iwyu_path_and_options}"
        )
    endif ()
endmacro()

function(_WEBKIT_TARGET_LINK_FRAMEWORK_INTO target_name framework _public_frameworks_var _private_frameworks_var)
    set_property(GLOBAL PROPERTY ${framework}_LINKED_INTO ${target_name})

    get_property(_framework_public_frameworks GLOBAL PROPERTY ${framework}_FRAMEWORKS)
    foreach (dependency IN LISTS ${_framework_public_frameworks})
        set(${_public_frameworks_var} "${${_public_frameworks_var}};${dependency}" PARENT_SCOPE)
    endforeach ()

    get_property(_framework_private_frameworks GLOBAL PROPERTY ${framework}_PRIVATE_FRAMEWORKS)
    foreach (dependency IN LISTS _framework_private_frameworks)
        set(${_private_frameworks_var} "${${_private_frameworks_var}};${dependency}" PARENT_SCOPE)
        _WEBKIT_TARGET_LINK_FRAMEWORK_INTO(${target_name} ${dependency} ${_public_frameworks_var} ${_private_frameworks_var})
    endforeach ()
endfunction()

macro(_WEBKIT_FRAMEWORK_LINK_FRAMEWORK _target_name)
    # Set the public libraries before modifying them when determining visibility.
    set_property(GLOBAL PROPERTY ${_target_name}_PUBLIC_LIBRARIES ${${_target_name}_LIBRARIES})

    set(_public_frameworks)
    set(_private_frameworks)

    foreach (framework IN LISTS ${_target_name}_FRAMEWORKS)
        get_property(_linked_into GLOBAL PROPERTY ${framework}_LINKED_INTO)
        if (_linked_into)
            list(APPEND _public_frameworks ${_linked_into})
        elseif (${framework}_LIBRARY_TYPE STREQUAL "SHARED")
            list(APPEND _public_frameworks ${framework})
        else ()
            list(APPEND _private_frameworks ${framework})
        endif ()
    endforeach ()

    # Recurse into the dependent frameworks
    if (_private_frameworks)
        list(REMOVE_DUPLICATES _private_frameworks)
    endif ()
    if (${_target_name}_LIBRARY_TYPE STREQUAL "SHARED")
        set_property(GLOBAL PROPERTY ${_target_name}_LINKED_INTO ${_target_name})
        foreach (framework IN LISTS _private_frameworks)
            _WEBKIT_TARGET_LINK_FRAMEWORK_INTO(${_target_name} ${framework} _public_frameworks _private_frameworks)
        endforeach ()
    endif ()

    # Add to the ${target_name}_LIBRARIES
    if (_public_frameworks)
        list(REMOVE_DUPLICATES _public_frameworks)
    endif ()
    foreach (framework IN LISTS _public_frameworks)
        # FIXME: https://bugs.webkit.org/show_bug.cgi?id=231774
        if (APPLE)
            list(APPEND ${_target_name}_PRIVATE_LIBRARIES WebKit::${framework})
        else ()
            list(APPEND ${_target_name}_LIBRARIES WebKit::${framework})
        endif ()
    endforeach ()

    # Add to the ${target_name}_PRIVATE_LIBRARIES
    if (_private_frameworks)
        list(REMOVE_DUPLICATES _private_frameworks)
    endif ()
    foreach (framework IN LISTS _private_frameworks)
        if (${_target_name}_LIBRARY_TYPE STREQUAL "SHARED")
            get_property(_linked_libraries GLOBAL PROPERTY ${framework}_PUBLIC_LIBRARIES)
            list(APPEND ${_target_name}_INTERFACE_LIBRARIES
                ${_linked_libraries}
            )
            list(APPEND ${_target_name}_INTERFACE_INCLUDE_DIRECTORIES
                ${${framework}_FRAMEWORK_HEADERS_DIR}
                ${${framework}_PRIVATE_FRAMEWORK_HEADERS_DIR}
            )
            list(APPEND ${_target_name}_PRIVATE_LIBRARIES WebKit::${framework})
            if (${framework}_LIBRARY_TYPE STREQUAL "OBJECT")
                list(APPEND ${_target_name}_PRIVATE_LIBRARIES "$<FILTER:$<TARGET_OBJECTS:${framework}>,EXCLUDE,\\.(g|p)ch$>")
                if (TARGET ${framework}_c)
                    list(APPEND ${_target_name}_PRIVATE_LIBRARIES "$<FILTER:$<TARGET_OBJECTS:${framework}_c>,EXCLUDE,\\.(g|p)ch$>")
                endif ()
            endif ()
        else ()
            list(APPEND ${_target_name}_LIBRARIES WebKit::${framework})
        endif ()
    endforeach ()

    set_property(GLOBAL PROPERTY ${_target_name}_FRAMEWORKS ${_public_frameworks})
    set_property(GLOBAL PROPERTY ${_target_name}_PRIVATE_FRAMEWORKS ${_private_frameworks})
endmacro()

macro(_WEBKIT_TARGET_LINK_FRAMEWORK _target)
    foreach (framework IN LISTS ${_target}_FRAMEWORKS)
        get_property(_linked_into GLOBAL PROPERTY ${framework}_LINKED_INTO)

        # See if the target is linking a framework that the specified framework is already linked into
        if ((NOT _linked_into) OR (${framework} STREQUAL ${_linked_into}) OR (NOT ${_linked_into} IN_LIST ${_target}_FRAMEWORKS))
            list(APPEND ${_target}_PRIVATE_LIBRARIES WebKit::${framework})

            # The WebKit:: alias targets do not propagate OBJECT libraries so the
            # underyling library's objects are explicitly added to link properly
            if (TARGET ${framework} AND ${framework}_LIBRARY_TYPE STREQUAL "OBJECT")
                list(APPEND ${_target}_PRIVATE_LIBRARIES "$<FILTER:$<TARGET_OBJECTS:${framework}>,EXCLUDE,\\.(g|p)ch$>")
                if (TARGET ${framework}_c)
                    list(APPEND ${_target}_PRIVATE_LIBRARIES "$<FILTER:$<TARGET_OBJECTS:${framework}_c>,EXCLUDE,\\.(g|p)ch$>")
                endif ()
            endif ()
        endif ()
    endforeach ()
endmacro()

macro(_WEBKIT_LIBRARY_LINK_FRAMEWORK _target)
    # See if the library is SHARED and if so just link frameworks the same as executables
    if (${_target}_LIBRARY_TYPE STREQUAL SHARED)
        _WEBKIT_TARGET_LINK_FRAMEWORK(${_target})
    else ()
        # Include the framework headers but don't try and link the frameworks
        foreach (framework IN LISTS ${_target}_FRAMEWORKS)
            list(APPEND ${_target}_INCLUDE_DIRECTORIES
                ${${framework}_FRAMEWORK_HEADERS_DIR}
                ${${framework}_PRIVATE_FRAMEWORK_HEADERS_DIR}
            )
        endforeach ()
    endif ()
endmacro()

macro(_WEBKIT_TARGET_INTERFACE _target)
    add_library(${_target}_PostBuild INTERFACE)
    target_link_libraries(${_target}_PostBuild INTERFACE ${${_target}_INTERFACE_LIBRARIES})
    target_include_directories(${_target}_PostBuild INTERFACE ${${_target}_INTERFACE_INCLUDE_DIRECTORIES})
    if (${_target}_INTERFACE_DEPENDENCIES)
        add_dependencies(${_target}_PostBuild ${${_target}_INTERFACE_DEPENDENCIES})
    endif ()
    if (NOT ${_target}_LIBRARY_TYPE STREQUAL "SHARED")
        target_compile_definitions(${_target}_PostBuild INTERFACE "STATICALLY_LINKED_WITH_${_target}")
    endif ()
    add_library(WebKit::${_target} ALIAS ${_target}_PostBuild)
endmacro()

macro(WEBKIT_FRAMEWORK _target)
    _WEBKIT_FRAMEWORK_LINK_FRAMEWORK(${_target})
    _WEBKIT_TARGET(${_target})
    _WEBKIT_TARGET_ANALYZE(${_target})

    # Apply PGO compile flags only to library targets (not executables) to avoid duplicate symbol errors
    # Link flags are applied globally via CMAKE_SHARED_LINKER_FLAGS for LTO compatibility
    if (PGO_COMPILE_OPTIONS)
        target_compile_options(${_target} PRIVATE ${PGO_COMPILE_OPTIONS})
    endif ()

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

    _WEBKIT_TARGET_INTERFACE(${_target})
endmacro()

macro(WEBKIT_LIBRARY _target)
    _WEBKIT_LIBRARY_LINK_FRAMEWORK(${_target})
    _WEBKIT_TARGET(${_target})
    _WEBKIT_TARGET_ANALYZE(${_target})

    # Apply PGO compile flags only to library targets (not executables) to avoid duplicate symbol errors
    # Link flags are applied globally via CMAKE_SHARED_LINKER_FLAGS for LTO compatibility
    if (PGO_COMPILE_OPTIONS)
        target_compile_options(${_target} PRIVATE ${PGO_COMPILE_OPTIONS})
    endif ()

    if (${_target}_OUTPUT_NAME)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${${_target}_OUTPUT_NAME})
    endif ()

    _WEBKIT_TARGET_INTERFACE(${_target})
endmacro()

macro(WEBKIT_EXECUTABLE _target)
    _WEBKIT_TARGET_LINK_FRAMEWORK(${_target})
    _WEBKIT_TARGET(${_target})
    _WEBKIT_TARGET_ANALYZE(${_target})

    if (${_target}_OUTPUT_NAME)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${${_target}_OUTPUT_NAME})
    endif ()
endmacro()

function(WEBKIT_COPY_FILES target_name)
    set(options FLATTENED NO_SYMLINK)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(files ${opt_FILES})
    set(dst_files)
    foreach (file IN LISTS files)
        if (IS_ABSOLUTE ${file})
            set(src_file ${file})
        else ()
            set(src_file ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        endif ()
        if (opt_FLATTENED)
            get_filename_component(filename ${file} NAME)
            set(dst_file ${opt_DESTINATION}/${filename})
        else ()
            get_filename_component(file_dir ${file} DIRECTORY)
            file(MAKE_DIRECTORY ${opt_DESTINATION}/${file_dir})
            set(dst_file ${opt_DESTINATION}/${file})
        endif ()
        # On macOS, symlink instead of copy so #import deduplicates headers reachable
        # via both forwarded (<WebKit/X.h>) and source-tree paths.
        # NO_SYMLINK for destinations post-processed in-place (e.g. ANGLE headers).
        if (APPLE AND NOT opt_NO_SYMLINK)
            add_custom_command(OUTPUT ${dst_file}
                COMMAND ${CMAKE_COMMAND} -E create_symlink ${src_file} ${dst_file}
                MAIN_DEPENDENCY ${file}
                VERBATIM
            )
        else ()
            add_custom_command(OUTPUT ${dst_file}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${src_file} ${dst_file}
                MAIN_DEPENDENCY ${file}
                VERBATIM
            )
        endif ()
        list(APPEND dst_files ${dst_file})
    endforeach ()
    add_custom_target(${target_name} ALL DEPENDS ${dst_files})
endfunction()

function(WEBKIT_SYMLINK_FILES target_name)
    set(options FLATTENED)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(files ${opt_FILES})
    set(dst_files)
    file(MAKE_DIRECTORY ${opt_DESTINATION})
    foreach (file IN LISTS files)
        if (IS_ABSOLUTE ${file})
            set(src_file ${file})
        else ()
            set(src_file ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        endif ()
        if (opt_FLATTENED)
            get_filename_component(filename ${file} NAME)
            set(dst_file ${opt_DESTINATION}/${filename})
        else ()
            get_filename_component(file_dir ${file} DIRECTORY)
            file(MAKE_DIRECTORY ${opt_DESTINATION}/${file_dir})
            set(dst_file ${opt_DESTINATION}/${file})
        endif ()
        add_custom_command(OUTPUT ${dst_file}
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${src_file} ${dst_file}
            MAIN_DEPENDENCY ${file}
            VERBATIM
        )
        list(APPEND dst_files ${dst_file})
    endforeach ()
    add_custom_target(${target_name} ALL DEPENDS ${dst_files})
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

macro(WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER _target _module_name _interop_module_path _output_header)
    if (SWIFT_REQUIRED)
        set_target_properties(${_target} PROPERTIES Swift_MODULE_NAME ${_module_name})
        # Ask swiftc where to find the header files which support C/C++ builds
        # Right now this macro is used only once; if it's used more often then
        # we should abstract this so it's executed only once.
        execute_process(
            COMMAND ${ORIGINAL_Swift_COMPILER} -print-target-info
            OUTPUT_VARIABLE _swift_target_info
        )
        string(JSON _swift_target_paths GET ${_swift_target_info} "paths")
        string(JSON _swift_runtime_resource_path GET ${_swift_target_paths} "runtimeResourcePath")
        target_include_directories(${_target} SYSTEM AFTER PRIVATE "${_swift_runtime_resource_path}")
        # Swift C++-interop objects auto-link swiftCxx/swiftCxxStdlib; consumers
        # linked by clang++ need this search path to satisfy those directives.
        string(JSON _swift_runtime_library_path GET ${_swift_target_paths} "runtimeLibraryPaths" 0)
        target_link_directories(${_target} INTERFACE "${_swift_runtime_library_path}")
        # Expose the path as a compile definition so the bubblewrap sandbox
        # (BubblewrapLauncher.cpp) can bind-mount it into the child process filesystem.
        target_compile_definitions(${_target} PRIVATE "WEBKIT_SWIFT_STDLIB_LIBRARY_PATH=\"${_swift_runtime_library_path}\"")

        # Assemble arguments which need to be passed to swiftc.
        # Add WebKit's various feature flags as -D directives to the Swift compiler.
        GET_WEBKIT_CONFIG_VARIABLES(_swift_definitions)
        list(TRANSFORM _swift_definitions PREPEND "-D")
        set(_swift_options ${_swift_definitions})
        set(_swift_xcc_options "")
        foreach (item IN LISTS _swift_options)
            list(APPEND _swift_xcc_options "-Xcc" ${item})
        endforeach ()
        get_directory_property(_dir_defs COMPILE_DEFINITIONS)
        foreach (_def IN LISTS _dir_defs)
            list(APPEND _swift_xcc_options "-Xcc" "-D${_def}")
        endforeach ()
        # Other options needed by Swift for C++ interop, including the location
        # of the modulemap and hader for WebKit's internal "APIs" which we
        # make available from C++ to Swift.
        list(APPEND _swift_options "-cxx-interoperability-mode=default" "-Xcc" "-std=c++2b" "-enable-upcoming-feature" "InternalImportsByDefault" "-Xcc" "-I${_interop_module_path}")
        # On non-Apple platforms, Swift's embedded clang doesn't automatically search
        # the compiler's C++ standard library headers (e.g. <coroutine> lives in /usr/include/c++/15/).
        # Pass them explicitly so the wtf umbrella module can include them.
        # Exclude GCC's architecture-specific lib directory (e.g. /usr/lib/gcc/aarch64-linux-gnu/15/include):
        # it contains GCC-specific intrinsic headers (arm_neon.h, etc.) that use GCC builtins
        # unknown to Swift's embedded Clang. Clang provides its own compatible versions in its
        # resource directory and will find them automatically without an explicit -I path.
        if (NOT APPLE)
            foreach (_dir IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
                if (NOT _dir MATCHES "^/usr/lib/gcc/")
                    list(APPEND _swift_options "-Xcc" "-I${_dir}")
                endif ()
            endforeach ()
        endif ()
        # swiftc spawns swift-plugin-server under sandbox-exec to expand macros
        # (e.g. SwiftUI @State). When the cmake build itself runs inside an
        # outer sandbox that disallows nested sandbox_apply, macro expansion
        # fails with "external macro implementation type ... could not be
        # found". -disable-sandbox skips the inner sandbox; the macros are
        # WebKit's own, so the isolation it provides isn't load-bearing here.
        list(APPEND _swift_options "-disable-sandbox")
        if (NOT (PORT STREQUAL GTK OR PORT STREQUAL WPE))
            # This does not yet work on non-Apple platforms for reasons yet to be determined.
            list(APPEND _swift_options "-explicit-module-build")
            # -explicit-module-build makes swiftc scan and compile every transitive
            # SDK Clang module to .pcm before typechecking. Without a fixed cache
            # path each invocation does that into a private temp dir and discards
            # it, so the -typecheck/-emit-clang-header pass below and cmake's own
            # Swift compile each pay the full SDK-module cold cost, every build.
            # Pin the cache so the second invocation -- and every later rebuild --
            # reuses the first one's .pcm set.
            list(APPEND _swift_options "-module-cache-path" "${CMAKE_BINARY_DIR}/SwiftModuleCache")
            set_property(DIRECTORY "${CMAKE_BINARY_DIR}" APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/SwiftModuleCache")
        endif ()
        # We'll use these options both for mainstream cmake invocations of swiftc (here)
        # and for our own invocation to output an interoperability .h file (later).
        # target_compile_options deduplicates repeated tokens, so collapse each
        # -Xcc <arg> into a single SHELL: entry to keep the pair together.
        # https://bugs.webkit.org/show_bug.cgi?id=312105
        set(_swift_only_options "")
        set(_pending_xcc FALSE)
        foreach (_opt IN LISTS _swift_options)
            if (_pending_xcc)
                list(APPEND _swift_only_options "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc ${_opt}>")
                set(_pending_xcc FALSE)
            elseif (_opt STREQUAL "-Xcc")
                set(_pending_xcc TRUE)
            else ()
                list(APPEND _swift_only_options "$<$<COMPILE_LANGUAGE:Swift>:${_opt}>")
            endif ()
        endforeach ()
        target_compile_options(${_target} PRIVATE ${_swift_only_options})

        if (CMAKE_SYSTEM_NAME STREQUAL "iOS" AND CMAKE_OSX_SYSROOT)
            target_compile_options(${_target} PRIVATE
                "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks>")
            if (EXISTS "${CMAKE_OSX_SYSROOT}/usr/local/include/unicode_private.modulemap")
                target_compile_options(${_target} PRIVATE
                    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${CMAKE_OSX_SYSROOT}/usr/local/include>"
                    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -fmodule-map-file=${CMAKE_OSX_SYSROOT}/usr/local/include/unicode_private.modulemap>")
            endif ()
        endif ()
        if (WEBKIT_ADDITIONS_COMPILE_PATH)
            target_compile_options(${_target} PRIVATE
                "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${WEBKIT_ADDITIONS_COMPILE_PATH}>")
        elseif (WEBKIT_ADDITIONS_INCLUDE_PATH)
            target_compile_options(${_target} PRIVATE
                "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -isystem${WEBKIT_ADDITIONS_INCLUDE_PATH}>")
        endif ()

        # cmake's Swift interop does not respect CMAKE_SHARED_LINKER_FLAGS, so let's pass
        # on those that we can.
        # rdar://155519819
        string(REPLACE " " ";" CMAKE_SHARED_LINKER_FLAGS_SPLIT "${CMAKE_SHARED_LINKER_FLAGS}")
        foreach (_flag IN ITEMS ${CMAKE_SHARED_LINKER_FLAGS_SPLIT})
            # We can only pass on -Wl flags.
            string(SUBSTRING ${_flag} 0 4 _prefix)
            if (${_prefix} STREQUAL "-Wl,")
                string(SUBSTRING ${_flag} 4 -1 _shorter_flag)
                # SHELL: keeps the -Xlinker/argument pair together; without it
                # CMake deduplicates the repeated -Xlinker tokens.
                # https://bugs.webkit.org/show_bug.cgi?id=312105
                target_compile_options(${_target} PUBLIC "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xlinker ${_shorter_flag}>")
            endif ()
        endforeach ()

        if (DEFINED ${_target}_SWIFT_TYPECHECK_SOURCES)
            set(_swift_sources ${${_target}_SWIFT_TYPECHECK_SOURCES})
        else ()
            set(_swift_sources $<TARGET_PROPERTY:${_target},SOURCES>)
            set(_swift_sources $<FILTER:${_swift_sources},INCLUDE,\\.swift$>)
        endif ()

        cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR include OUTPUT_VARIABLE _header_base_path)
        cmake_path(APPEND _header_base_path ${_output_header} OUTPUT_VARIABLE _header_path)
        cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR "${_target}.emit-module.d" OUTPUT_VARIABLE _depfile_path)

        # Allow targets to override include directories for Swift (e.g. to exclude
        # directories containing conflicting module.modulemap files).
        if (DEFINED ${_target}_SWIFT_INCLUDE_DIRECTORIES AND NOT "${${_target}_SWIFT_INCLUDE_DIRECTORIES}" STREQUAL "")
            list(TRANSFORM ${_target}_SWIFT_INCLUDE_DIRECTORIES PREPEND "-I" OUTPUT_VARIABLE _swift_include_dirs)
        elseif (NOT DEFINED ${_target}_SWIFT_INCLUDE_DIRECTORIES)
            set(_swift_include_dirs $<LIST:TRANSFORM,$<TARGET_PROPERTY:${_target},INCLUDE_DIRECTORIES>,PREPEND,-I>)
        else ()
            set(_swift_include_dirs "")
        endif ()

        set(_swift_sdk_flag "")
        if (APPLE AND CMAKE_OSX_SYSROOT)
            set(_swift_sdk_flag -sdk ${CMAKE_OSX_SYSROOT})
        endif ()

        set(_swift_target_flag "")
        if (CMAKE_Swift_COMPILER_TARGET)
            set(_swift_target_flag -target ${CMAKE_Swift_COMPILER_TARGET})
        endif ()

        set(_swift_private_frameworks_flag "")
        if ((CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "visionOS") AND CMAKE_OSX_SYSROOT)
            set(_swift_private_frameworks_flag
                -Xcc -iframework${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks
                -F ${CMAKE_BINARY_DIR}
                -F ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks
            )
            if (EXISTS "${CMAKE_OSX_SYSROOT}/usr/local/include/unicode_private.modulemap")
                list(APPEND _swift_private_frameworks_flag
                    -Xcc -isystem${CMAKE_OSX_SYSROOT}/usr/local/include
                    -Xcc -fmodule-map-file=${CMAKE_OSX_SYSROOT}/usr/local/include/unicode_private.modulemap
                )
            endif ()
            set(_vfs_overlay "${CMAKE_BINARY_DIR}/swift-vfs-overlay.yaml")
            if (EXISTS "${_vfs_overlay}")
                list(APPEND _swift_private_frameworks_flag
                    -Xcc -ivfsoverlay -Xcc ${_vfs_overlay}
                )
            endif ()
        endif ()

        set(_swift_wka_flag "")
        if (WEBKIT_ADDITIONS_COMPILE_PATH)
            set(_swift_wka_flag -Xcc -isystem${WEBKIT_ADDITIONS_COMPILE_PATH})
        elseif (WEBKIT_ADDITIONS_INCLUDE_PATH)
            set(_swift_wka_flag -Xcc -isystem${WEBKIT_ADDITIONS_INCLUDE_PATH})
        endif ()

        set(_header_tmp_path "${_header_path}.tmp")
        add_custom_command(
            OUTPUT ${_header_path}
            DEPENDS ${_swift_sources}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMAND
                ${CMAKE_Swift_COMPILER} --original-swift-compiler=${ORIGINAL_Swift_COMPILER} -typecheck
                ${_swift_options}
                ${${_target}_SWIFT_EXTRA_OPTIONS}
                ${_swift_sdk_flag}
                ${_swift_target_flag}
                ${_swift_private_frameworks_flag}
                ${_swift_wka_flag}
                ${_swift_include_dirs}
                ${_swift_xcc_options}
                ${_swift_sources}
                -module-name ${_module_name}
                -Xfrontend -emit-clang-header-min-access -Xfrontend internal
                -emit-clang-header-path ${_header_tmp_path}
                -emit-dependencies
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different ${_header_tmp_path} ${_header_path}
            COMMAND
                ${CMAKE_COMMAND} -E rm -f ${_header_tmp_path}
            DEPFILE ${_depfile_path}
            COMMENT
                "Generating ${_target} C++ bindings to Swift at '${_header_path}'"
            COMMAND_EXPAND_LISTS)

        target_include_directories(${_target} PUBLIC ${_header_base_path})
        if (DEFINED ${_target}_SWIFT_HEADER_DEPENDS)
            # The -emit-clang-header pass only needs the staged headers it actually
            # reads, not the target's linked frameworks. When the caller names
            # those header-producing targets, wrap the command in its own
            # custom target so it does NOT inherit
            # cmake_object_order_depends_target_${_target} (which would gate it
            # on every link dependency) and can start as soon as headers exist.
            add_custom_target(${_target}_SwiftCxxHeader DEPENDS ${_header_path})
            add_dependencies(${_target}_SwiftCxxHeader ${${_target}_SWIFT_HEADER_DEPENDS})
            add_dependencies(${_target} ${_target}_SwiftCxxHeader)
        else ()
            target_sources(${_target} PRIVATE ${_header_path})
        endif ()
    endif ()
endmacro()
