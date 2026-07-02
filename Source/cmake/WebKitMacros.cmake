# This file is for macros that are used by multiple projects. If your macro is
# exclusively needed in only one subdirectory of Source (e.g. only needed by
# WebCore), then put it there instead.

macro(WEBKIT_COMPUTE_SOURCES _framework)
    set(_derivedSourcesPath ${${_framework}_DERIVED_SOURCES_DIR})

    foreach (_sourcesListFile IN LISTS ${_framework}_UNIFIED_SOURCE_LIST_FILES)
      if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}")
          set(_sourcesListInput "${CMAKE_CURRENT_SOURCE_DIR}/${_sourcesListFile}")
      else ()
          set(_sourcesListInput "${_derivedSourcesPath}/${_sourcesListFile}")
      endif ()
      if (${_framework}_UNIFIED_SOURCE_EXCLUDES)
          file(STRINGS "${_sourcesListInput}" _allLines)
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
          configure_file("${_sourcesListInput}" "${_derivedSourcesPath}/${_sourcesListFile}" COPYONLY)
      endif ()
      message(STATUS "Using source list file: ${_sourcesListFile}")

      list(APPEND _sourceListFileTruePaths "${_derivedSourcesPath}/${_sourcesListFile}")
    endforeach ()

    set(gusb_args --derived-sources-path ${_derivedSourcesPath} --source-tree-path ${CMAKE_CURRENT_SOURCE_DIR})
    list(APPEND gusb_args --ignore-header-groups)
    # Windows needs a larger bundle size because that helps keep WebCore.lib's size below the 4GB maximum in debug builds.
    if (MSVC AND ${_framework} STREQUAL "WebCore" AND ${_framework}_LIBRARY_TYPE STREQUAL "STATIC")
        list(APPEND gusb_args --max-bundle-size 16)
    endif ()
    if (${_framework} STREQUAL "WebCore")
        list(APPEND gusb_args --dense-bundle-filter "JS*=JSBindings" --dense-bundle-filter "bindings/*=JSBindings")
    endif ()
    if (DEFINED WEBKIT_MAX_BUNDLE_SIZE)
        list(APPEND gusb_args --max-bundle-size ${WEBKIT_MAX_BUNDLE_SIZE} --enforce-cost)
    endif ()

    if (ENABLE_UNIFIED_BUILDS)
        # One pass generates the bundles (stdout = files to compile) and writes the
        # bundled member list to a side file via --print-bundled-sources.
        set(_bundledSourcesFile "${CMAKE_CURRENT_BINARY_DIR}/${_framework}BundledSources.txt")
        execute_process(COMMAND ${Python_EXECUTABLE} ${WTF_SCRIPTS_DIR}/generate-unified-source-bundles.py
            ${gusb_args}
            --print-bundled-sources "${_bundledSourcesFile}"
            ${_sourceListFileTruePaths}
            RESULT_VARIABLE _resultTmp
            OUTPUT_VARIABLE _outputTmp)

        if (${_resultTmp})
             message(FATAL_ERROR "generate-unified-source-bundles.py exited with non-zero status, exiting")
        endif ()

        # Member sources folded into bundles: compiled via the bundle, so mark header-only.
        file(STRINGS "${_bundledSourcesFile}" _bundledSources)
        foreach (_sourceFileTmp IN LISTS _bundledSources)
            set_source_files_properties(${_sourceFileTmp} PROPERTIES HEADER_FILE_ONLY ON)
            list(APPEND ${_framework}_HEADERS ${_sourceFileTmp})
        endforeach ()
        unset(_sourceFileTmp)

        foreach (_file IN LISTS _outputTmp)
            # Disambiguate top-level generated sources in DerivedSources vs top-level regular sources.
            get_filename_component(_fileDir "${_file}" DIRECTORY)
            if (NOT _fileDir AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_file}")
                set(_file "${_derivedSourcesPath}/${_file}")
            endif ()
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
function(WEBKIT_ADD_PREFIX_HEADER _target _header)
    cmake_parse_arguments(PARSE_ARGV 2 _PCH "PREFIX_NO_CODEGEN" "" "PREFIX_LANGUAGES")
    if (NOT _PCH_PREFIX_LANGUAGES)
        message(FATAL_ERROR "WEBKIT_ADD_PREFIX_HEADER(${_target}): PREFIX_LANGUAGES is required")
    endif ()
    string(JOIN "," _pch_genex_langs ${_PCH_PREFIX_LANGUAGES})
    target_precompile_headers(${_target} PRIVATE
        "$<$<COMPILE_LANGUAGE:${_pch_genex_langs}>:${CMAKE_CURRENT_SOURCE_DIR}/${_header}>")
    _WEBKIT_PCH_STUB_NO_TIMESTAMP(${_target} ${_PCH_PREFIX_LANGUAGES})
    _WEBKIT_ADD_PCH_OBJECT(${_target} ${ARGN})
endfunction()

# REUSE_FROM is only safe where producer and consumer compile flags match exactly.
# That fails on GCC (per-target BUILDING_* define -> -Werror=invalid-pch) and on
# ELF clang when an executable reuses a library PCH (-fPIE vs -fPIC). Fall back
# to a per-target prefix header on those ports.
function(WEBKIT_REUSE_PREFIX_HEADER _target _from _header)
    if (COMPILER_IS_CLANG AND APPLE)
        target_precompile_headers(${_target} REUSE_FROM ${_from})
    else ()
        WEBKIT_ADD_PREFIX_HEADER(${_target} ${_header} PREFIX_NO_CODEGEN ${ARGN})
    endif ()
endfunction()

function(_WEBKIT_ADD_PCH_OBJECT _target)
    if (NOT (COMPILER_IS_CLANG AND APPLE))
        return()
    endif ()
    cmake_parse_arguments(PARSE_ARGV 1 _PO "PREFIX_NO_CODEGEN" "" "PREFIX_LANGUAGES")
    set(_stub_flags "-fpch-debuginfo;-Xclang;-building-pch-with-obj")
    if (NOT _PO_PREFIX_NO_CODEGEN)
        list(PREPEND _stub_flags "-fpch-codegen")
    endif ()
    get_target_property(_pch_bin_dir ${_target} BINARY_DIR)
    list(FILTER _PO_PREFIX_LANGUAGES INCLUDE REGEX "^(CXX|OBJCXX)$")
    foreach (_pch_lang IN LISTS _PO_PREFIX_LANGUAGES)
        _WEBKIT_PCH_PATHS_FOR_LANGUAGE(${_pch_lang} _pch_src_ext _pch_stub_ext _pch_stem)
        set_property(SOURCE "${_pch_bin_dir}/CMakeFiles/${_target}.dir/${_pch_stem}.${_pch_stub_ext}"
            APPEND PROPERTY COMPILE_OPTIONS "${_stub_flags}")
        set(_pch_obj_src "${CMAKE_CURRENT_BINARY_DIR}/${_target}_pch_obj.${_pch_src_ext}")
        if (NOT EXISTS "${_pch_obj_src}")
            file(WRITE "${_pch_obj_src}" "// PCH object for ${_target} (${_pch_lang})\n")
        endif ()
        target_sources(${_target} PRIVATE "${_pch_obj_src}")
        set_source_files_properties("${_pch_obj_src}" PROPERTIES
            COMPILE_OPTIONS "-Xclang;-building-pch-with-obj;-fvisibility-inlines-hidden"
            SKIP_UNITY_BUILD_INCLUSION ON)
    endforeach ()
endfunction()

function(_WEBKIT_PCH_STUB_NO_TIMESTAMP _target)
    if (NOT COMPILER_IS_CLANG)
        return()
    endif ()
    get_target_property(_pch_bin_dir ${_target} BINARY_DIR)
    foreach (_lang ${ARGN})
        _WEBKIT_PCH_PATHS_FOR_LANGUAGE(${_lang} _src_ext _stub_ext _pch_stem)
        set_property(SOURCE "${_pch_bin_dir}/CMakeFiles/${_target}.dir/${_pch_stem}.${_stub_ext}"
            APPEND PROPERTY COMPILE_OPTIONS "-Xclang;-fno-pch-timestamp")
    endforeach ()
endfunction()

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

function(WEBKIT_ADD_PREFIX_HEADER_WITH_PARENT _target _base_target _header _parent_header)
    if (COMPILER_IS_CLANG)
        string(JOIN "," _lang_genex ${ARGN})
        target_precompile_headers(${_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:${_lang_genex}>:${CMAKE_CURRENT_SOURCE_DIR}/${_header}>")
        if (APPLE)
            # FIXME: Upstream clang does not appear to propagate parent-PCH state to consumers
            # of the child PCH (webkit.org/b/314763). Until that is root-caused, build the child
            # prefix as a standalone PCH on non-Apple clang; the child header #includes its parent.
            get_target_property(_chain_bin_dir ${_target} BINARY_DIR)
            foreach (_lang ${ARGN})
                _WEBKIT_PCH_PATHS_FOR_LANGUAGE(${_lang} _src_ext _stub_ext _pch_stem)
                set(_base_pch "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${_base_target}.dir/${_pch_stem}.pch")
                set(_chain_stub "${_chain_bin_dir}/CMakeFiles/${_target}.dir/${_pch_stem}.${_stub_ext}")
                set_source_files_properties(${_chain_stub} PROPERTIES
                    COMPILE_OPTIONS "-Xclang;-include-pch;-Xclang;${_base_pch}"
                    OBJECT_DEPENDS "${_base_pch}")
            endforeach ()
        endif ()
        _WEBKIT_PCH_STUB_NO_TIMESTAMP(${_target} ${ARGN})
        _WEBKIT_ADD_PCH_OBJECT(${_target} PREFIX_NO_CODEGEN PREFIX_LANGUAGES ${ARGN})
    else ()
        WEBKIT_ADD_PREFIX_HEADER(${_target} ${_parent_header} PREFIX_LANGUAGES ${ARGN})
    endif ()
endfunction()

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
    # Subtargets claim sources first-match-wins, so define HEADER_GROUPS subtargets
    # before any DIRS subtarget whose directories overlap.
    cmake_parse_arguments(_arg "" "PREFIX;CHAIN_PARENT" "PREFIX_LANGUAGES;DIRS;HEADER_GROUPS" ${ARGN})
    if (${_target}_FINALIZED)
        message(FATAL_ERROR "WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX(${_target} ${_subtarget}) must be called before WEBKIT_FRAMEWORK(${_target})")
    endif ()
    if (NOT _arg_CHAIN_PARENT)
        set(_arg_CHAIN_PARENT ${_target})
    endif ()
    if (COMPILER_IS_CLANG AND NOT MSVC AND NOT CMAKE_DISABLE_PRECOMPILE_HEADERS)
        set(_src_exts)
        foreach (_lang IN LISTS _arg_PREFIX_LANGUAGES)
            _WEBKIT_PCH_PATHS_FOR_LANGUAGE(${_lang} _src_ext _stub_ext _pch_stem)
            list(APPEND _src_exts ${_src_ext})
        endforeach ()
        string(JOIN "|" _src_exts ${_src_exts})
        set(${_subtarget}_SOURCES ${${_target}_SOURCES})
        list(FILTER ${_subtarget}_SOURCES INCLUDE REGEX "\\.(${_src_exts})$")
        if (_arg_DIRS)
            string(JOIN "|" _dirs ${_arg_DIRS})
            list(FILTER ${_subtarget}_SOURCES INCLUDE REGEX "(^|[-/])(${_dirs})[-/]")
        endif ()
        if (_arg_HEADER_GROUPS)
            string(JOIN "|" _groups ${_arg_HEADER_GROUPS})
            list(FILTER ${_subtarget}_SOURCES INCLUDE REGEX "-header-(${_groups})\\.")
        endif ()
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
            WEBKIT_ADD_PREFIX_HEADER_WITH_PARENT(${_subtarget} ${_arg_CHAIN_PARENT} ${_arg_PREFIX} "" ${_arg_PREFIX_LANGUAGES})
        endif ()
    endif ()
    unset(_arg_CHAIN_PARENT)
    unset(_arg_PREFIX)
    unset(_arg_PREFIX_LANGUAGES)
    unset(_arg_DIRS)
    unset(_arg_HEADER_GROUPS)
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
    if (USE_HEADER_MAPS)
        WEBKIT_WRITE_HEADER_MAP(${_target}
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${_target}-project-headers.hmap
            FILES ${${_target}_PROJECT_HEADERS}
            QUOTED BRACKETED
        )
        WEBKIT_WRITE_HEADER_MAP(${_target}
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${_target}-framework-headers.hmap
            FILES
                ${${_target}_PUBLIC_FRAMEWORK_HEADERS} ${${_target}_PUBLIC_HEADERS}
                ${${_target}_PRIVATE_FRAMEWORK_HEADERS} ${${_target}_PRIVATE_HEADERS}
            QUOTED BRACKETED
        )
        include_directories(BEFORE
            ${CMAKE_CURRENT_BINARY_DIR}/${_target}-project-headers.hmap
            ${CMAKE_CURRENT_BINARY_DIR}/${_target}-framework-headers.hmap
        )
        target_include_directories(${_target} BEFORE PUBLIC
            ${CMAKE_CURRENT_BINARY_DIR}/${_target}-framework-headers.hmap
        )
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
    if (${_target}_INTERFACE_LINK_DEPENDS)
        set_property(TARGET ${_target}_PostBuild PROPERTY
            INTERFACE_LINK_DEPENDS ${${_target}_INTERFACE_LINK_DEPENDS})
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

# Delete staged files not in FILES, so a renamed or removed header drops its old
# copy instead of lingering and colliding as a duplicate in a Clang umbrella
# module. Only safe when the calling step owns the destination exclusively.
# `flattened` matches by basename when TRUE, by relative path otherwise.
function(WEBKIT_PRUNE_STALE_DESTINATION destination flattened)
    set(_expected)
    foreach (file IN LISTS ARGN)
        if (flattened)
            get_filename_component(_rel ${file} NAME)
        else ()
            set(_rel ${file})
        endif ()
        list(APPEND _expected ${_rel})
    endforeach ()
    file(GLOB_RECURSE _existing RELATIVE ${destination} ${destination}/*)
    foreach (_entry IN LISTS _existing)
        if (NOT _entry IN_LIST _expected)
            file(REMOVE ${destination}/${_entry})
        endif ()
    endforeach ()
endfunction()

function(WEBKIT_COPY_FILES target_name)
    set(options FLATTENED NO_SYMLINK PRUNE_STALE)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(files ${opt_FILES})
    set(dst_files)

    if (opt_PRUNE_STALE)
        WEBKIT_PRUNE_STALE_DESTINATION(${opt_DESTINATION} "${opt_FLATTENED}" ${files})
    endif ()

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
                MAIN_DEPENDENCY ${src_file}
                VERBATIM
            )
        else ()
            add_custom_command(OUTPUT ${dst_file}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${src_file} ${dst_file}
                MAIN_DEPENDENCY ${src_file}
                VERBATIM
            )
        endif ()
        list(APPEND dst_files ${dst_file})
    endforeach ()
    add_custom_target(${target_name} ALL DEPENDS ${dst_files})
endfunction()

function(WEBKIT_SYMLINK_FILES target_name)
    set(options FLATTENED PRUNE_STALE)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(files ${opt_FILES})
    set(dst_files)
    file(MAKE_DIRECTORY ${opt_DESTINATION})

    if (opt_PRUNE_STALE)
        WEBKIT_PRUNE_STALE_DESTINATION(${opt_DESTINATION} "${opt_FLATTENED}" ${files})
    endif ()

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
            MAIN_DEPENDENCY ${src_file}
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

function(WEBKIT_ADD_TARGET_UNSAFE_BUFFER_WARNINGS _target)
    if (ENABLE_UNSAFE_BUFFER_USAGE_WARNING AND WEBKIT_UNSAFE_BUFFER_WARNING_FLAGS)
        WEBKIT_ADD_TARGET_CXX_FLAGS(${_target} ${WEBKIT_UNSAFE_BUFFER_WARNING_FLAGS})
    endif ()
endfunction()

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

function(_WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS _outvar)
    # All Swift C++-interop targets pass the same -Xcc -D set so the clang
    # importer's module-cache hash matches and bmalloc/wtf/SDK PCMs build once.
    # -I/-isystem/-fmodule-map-file/-fvisibility are not in the hash and stay
    # per-target. Per-target target_compile_definitions are intentionally NOT
    # forwarded here; the wrapper no longer mirrors plain -D to -Xcc.
    set(_flags
        -DENABLE_WEBGPU_SWIFT=1
        -DJS_EXPORT_PRIVATE=
        -DNODELETE=
        -DPAL_EXPORT=
        -DUCHAR_TYPE=char16_t
        -DWEBCORE_EXPORT=
        -DWEBCORE_TESTSUPPORT_EXPORT=
        -DWK_EXPORT=
        -DWTF_EXPORT_PRIVATE=
        -D__WEBGPU__
    )
    # iOS WebKit_Internal headers gate textual #imports behind this macro that
    # trip strict cross-module-import-visibility checks (bug 312083).
    if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND _flags -DWK_SUPPORTS_SWIFT_OBJCXX_INTEROP=1)
    endif ()
    if (APPLE)
        # Normalize the LangOpt that WebGPU's SafeInteropWrappers enables
        # implicitly so PAL/WebKit hash to the same module-cache dir.
        list(APPEND _flags -fexperimental-bounds-safety-attributes)
    endif ()
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
    if (CMAKE_CXX_FLAGS_${_bt} MATCHES "NDEBUG" OR CMAKE_CXX_FLAGS MATCHES "NDEBUG")
        list(APPEND _flags -DNDEBUG -DRELEASE_WITHOUT_OPTIMIZATIONS)
    endif ()
    # Globals from add_definitions() (BUILDING_WEBKIT=1, PAS_BMALLOC=1,
    # _LIBCPP_HARDENING_MODE=...). Read from a fixed directory so every caller
    # sees the same set regardless of its own add_definitions().
    get_directory_property(_dir_defs DIRECTORY "${CMAKE_SOURCE_DIR}/Source" COMPILE_DEFINITIONS)
    foreach (_d IN LISTS _dir_defs)
        if (NOT _d MATCHES "^(BUILDING_WITH_CMAKE|HAVE_CONFIG_H)($|=)")
            list(APPEND _flags "-D${_d}")
        endif ()
    endforeach ()
    # cmakeconfig.h's ENABLE_/HAVE_/USE_ values are NOT enumerated here. They
    # come in via the per-target @-response file generated by
    # _webkit_generate_platform_swift_args, which preprocesses wtf/Platform.h
    # so the importer sees the same effective definitions C++ TUs do
    # (including Platform.h-derived flags like HAVE_MATERIAL_HOSTING that
    # cmakeconfig.h doesn't track).
    set(${_outvar} ${_flags} PARENT_SCOPE)
endfunction()

# Generates ${_resp_path}, a swiftc @-response file with the platform-derived
# flags. Mirrors the Xcode "Generate Swift platform args" build phase.
# Format (one token per line): -DNAME for truthy HAVE_/USE_/ENABLE_/
# WTF_PLATFORM_/ASSERT_ macros from wtf/Platform.h, plus -Xcc -DNAME=VALUE
# for every cmakeconfig.h entry. DEPFILE re-runs it on Platform header changes.
function(_webkit_generate_platform_swift_args _target _resp_path _ordering_dep)
    set(_depfile "${_resp_path}.d")
    # Stable empty file; /dev/null's mtime is "now" and would always look dirty.
    set(_empty_input "${CMAKE_BINARY_DIR}/DerivedSources/empty.cpp")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/DerivedSources")
    if (NOT EXISTS "${_empty_input}")
        file(WRITE "${_empty_input}" "")
    endif ()
    set(_clang_cmd
        ${CMAKE_CXX_COMPILER}
        -x c++ -std=c++2b
        -E -P -dM
        -MD -MF "${_depfile}" -MT "${_resp_path}"
        -D __WK_GENERATING_PLATFORM_ARGS__
        # Mirrors Xcode's generate-platform-args: avoids wtf/Compiler.h's
        # #error in non-Debug since the preprocessor never sets __OPTIMIZE__.
        -D RELEASE_WITHOUT_OPTIMIZATIONS
        -I "${WTF_FRAMEWORK_HEADERS_DIR}"
        -I "${CMAKE_BINARY_DIR}"
        -include cmakeconfig.h
        -include wtf/Platform.h
    )
    if (CMAKE_OSX_SYSROOT)
        list(APPEND _clang_cmd "-isysroot" "${CMAKE_OSX_SYSROOT}")
    endif ()
    if (CMAKE_Swift_COMPILER_TARGET)
        list(APPEND _clang_cmd "-target" "${CMAKE_Swift_COMPILER_TARGET}")
    endif ()
    if (WEBKIT_ADDITIONS_INCLUDE_PATH)
        list(APPEND _clang_cmd "-I" "${WEBKIT_ADDITIONS_INCLUDE_PATH}")
    endif ()
    # Custom commands don't inherit add_compile_options, so mirror the -isystem
    # stub OptionsCocoa.cmake feeds C-family compiles: an empty AppleFeatures.h
    # for SDKs lacking it, which Platform.h's preprocessing needs.
    if (EXISTS "${CMAKE_BINARY_DIR}/generated-stubs")
        list(APPEND _clang_cmd "-isystem" "${CMAKE_BINARY_DIR}/generated-stubs")
    endif ()
    # NDEBUG affects ASSERT_ENABLED -> ENABLE_SECURITY_ASSERTIONS -> struct layouts.
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _build_type_upper)
    if (CMAKE_CXX_FLAGS_${_build_type_upper} MATCHES "NDEBUG" OR CMAKE_CXX_FLAGS MATCHES "NDEBUG")
        list(APPEND _clang_cmd "-DNDEBUG")
    endif ()
    list(APPEND _clang_cmd "${_empty_input}")

    # Order resp generation after the framework's headers are staged: the
    # clang preprocess -include's wtf/Platform.h from WTF_FRAMEWORK_HEADERS_DIR.
    # Targets declared after this macro call are skipped (not yet visible to
    # if(TARGET ...)); for current callers WTF_CopyHeaders is already created.
    set(_header_deps "")
    foreach (_lib IN ITEMS WTF "${_target}" ${${_target}_FRAMEWORKS})
        foreach (_suffix IN ITEMS _CopyHeaders _CopyPrivateHeaders)
            if (TARGET "${_lib}${_suffix}")
                list(APPEND _header_deps "${_lib}${_suffix}")
            endif ()
        endforeach ()
    endforeach ()
    list(REMOVE_DUPLICATES _header_deps)

    set(_script "${CMAKE_SOURCE_DIR}/Source/WTF/Scripts/generate-platform-args")
    add_custom_command(
        OUTPUT "${_resp_path}"
        COMMAND ${Python_EXECUTABLE}
            "${_script}"
            --cmake
            --output "${_resp_path}"
            --cmakeconfig "${CMAKE_BINARY_DIR}/cmakeconfig.h"
            --
            ${_clang_cmd}
        DEPFILE "${_depfile}"
        DEPENDS
            "${_script}"
            "${CMAKE_BINARY_DIR}/cmakeconfig.h"
            ${_ordering_dep}
            ${_header_deps}
        COMMENT "Generating ${_target} platform-swift-args.resp"
        VERBATIM
    )
endfunction()

function(_webkit_setup_swift_header_deps _target _stamp _header _resp)
    # Discover _CopyHeaders/_CopyPrivateHeaders targets for this target and its
    # direct framework dependencies. Called via cmake_language(DEFER CALL ...)
    # so targets declared after WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER
    # (e.g. ${_target}_CopyHeaders itself) are visible to if(TARGET ...).
    set(_candidates "${_target}")
    if (DEFINED ${_target}_FRAMEWORKS)
        list(APPEND _candidates ${${_target}_FRAMEWORKS})
    endif ()
    set(_deps "")
    foreach (_lib IN LISTS _candidates)
        foreach (_suffix IN ITEMS _CopyHeaders _CopyPrivateHeaders)
            if (TARGET "${_lib}${_suffix}")
                list(APPEND _deps "${_lib}${_suffix}")
            endif ()
        endforeach ()
    endforeach ()
    list(REMOVE_DUPLICATES _deps)

    # Collect all generated (binary-dir) headers/sources that Swift may import.
    # ${_target}_HEADERS and ${_target}_DERIVED_SOURCES are fully populated by
    # the time this deferred function runs. We find those that live under
    # CMAKE_BINARY_DIR (i.e., produced by add_custom_command) and wire them into
    # the ${_target}_SwiftGeneratedDeps placeholder target created at macro time.
    # This covers both the header-emission add_custom_command and the main Swift
    # compilation
    set(_generated_files "")
    foreach (_f IN LISTS ${_target}_HEADERS ${_target}_DERIVED_SOURCES)
        cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${_f}" NORMALIZE _in_build)
        if (_in_build)
            list(APPEND _generated_files "${_f}")
        endif ()
    endforeach ()
    if (_generated_files)
        # File-level DEPENDS must be fixed at target-creation time, so create a
        # separate inner target and chain it into the placeholder.
        add_custom_target(${_target}_SwiftGenFileOrderDeps DEPENDS ${_generated_files})
        add_dependencies(${_target}_SwiftGeneratedDeps ${_target}_SwiftGenFileOrderDeps)
    endif ()
    # Ensure the main Swift compilation also waits for all generated files.
    add_dependencies(${_target} ${_target}_SwiftGeneratedDeps)

    add_custom_target(${_target}_SwiftCxxHeaderStamp DEPENDS ${_stamp})
    add_dependencies(${_target}_SwiftCxxHeader ${_target}_SwiftCxxHeaderStamp)
    if (_deps)
        add_dependencies(${_target}_SwiftCxxHeader ${_deps})
        add_dependencies(${_target}_SwiftGeneratedDeps ${_deps})
    endif ()

    # Pre-CMP0157 CMake compiles .swift files inside ${_target}'s link rule, which is a
    # circular dependency. This workaround retains the old "compile twice" behavior,
    # with the first compilation producing a throw-away .a file for the purpose of
    # generating the C++ interop header.
    if (NOT POLICY CMP0157)
        get_target_property(_swift_srcs ${_target} SOURCES)
        list(FILTER _swift_srcs INCLUDE REGEX "\\.swift$")
        if (_swift_srcs)
            get_target_property(_module_name ${_target} Swift_MODULE_NAME)
            add_library(${_target}_SwiftCompile STATIC EXCLUDE_FROM_ALL ${_swift_srcs})
            set_target_properties(${_target}_SwiftCompile PROPERTIES Swift_MODULE_NAME ${_module_name})
            target_compile_options(${_target}_SwiftCompile PRIVATE $<TARGET_PROPERTY:${_target},COMPILE_OPTIONS>)
            target_compile_definitions(${_target}_SwiftCompile PRIVATE $<TARGET_PROPERTY:${_target},COMPILE_DEFINITIONS>)
            target_include_directories(${_target}_SwiftCompile PRIVATE $<TARGET_PROPERTY:${_target},INCLUDE_DIRECTORIES>)
            # Emit the interop header from the helper only: it is the sole writer
            # of WebKit-Swift-Generated.h.tmp in the legacy path and produces the
            # swiftmodule the copy depends on, so the copy is ordered after the
            # single write. ${_target} deliberately does not emit it here.
            if (DEFINED ${_target}_SWIFT_EMIT_HEADER_FLAGS)
                target_compile_options(${_target}_SwiftCompile PRIVATE ${${_target}_SWIFT_EMIT_HEADER_FLAGS})
            endif ()
            add_dependencies(${_target}_SwiftCompile ${_target}_SwiftGeneratedDeps)
            file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/swift-link)
            set_target_properties(${_target} PROPERTIES Swift_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/swift-link)
        endif ()
    endif ()

    add_dependencies(${_target}_SwiftInterop ${_target}_SwiftCxxHeader)
endfunction()

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
        # The platform-derived feature flags (HAVE_/USE_/ENABLE_/WTF_PLATFORM_/
        # ASSERT_) plus the cmakeconfig.h seed defines for the clang importer
        # are produced as a build-time @-response file. The custom command that
        # writes it is created later (after the SwiftGeneratedDeps placeholder
        # exists); we just thread the @-flag here so it ends up in the swiftc
        # invocation that CMake assembles from target_compile_options.
        set(_resp_path "${CMAKE_CURRENT_BINARY_DIR}/${_target}.platform-swift-args.resp")
        set(_swift_options "@${_resp_path}")
        # Other options needed by Swift for C++ interop, including the location
        # of the modulemap and hader for WebKit's internal "APIs" which we
        # make available from C++ to Swift.
        # By default the interop module dir goes on Clang's include search path
        # (-Xcc -I) so the C++ interop importer can find module.modulemap there.
        # Targets where the modulemap must remain Swift-only (e.g. WebKit, whose
        # WebKit_Internal would otherwise be loaded twice and conflict with the
        # WebKit_Private framework module — matches Xcode's SWIFT_INCLUDE_PATHS
        # which is also Swift-only) can set
        # ${_target}_SWIFT_INTEROP_MODULE_PATH_SWIFT_ONLY to TRUE.
        list(APPEND _swift_options "-cxx-interoperability-mode=default" "-Xcc" "-std=c++2b")
        _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS(_shared_cc_flags)
        foreach (_f IN LISTS _shared_cc_flags)
            list(APPEND _swift_options "-Xcc" "${_f}")
        endforeach ()
        # Match Xcode's CommonBase.xcconfig SWIFT_VERSION = 6.0. The importer's
        # apinotes version is keyed off the effective Swift language mode, so
        # this also keeps PAL/WebGPU/WebKit on the same module-cache hash.
        list(APPEND _swift_options "-swift-version" "6")
        if (${_target}_SWIFT_INTEROP_MODULE_PATH_SWIFT_ONLY)
            list(APPEND _swift_options "-I${_interop_module_path}")
        else ()
            list(APPEND _swift_options "-Xcc" "-I${_interop_module_path}")
        endif ()
        # InternalImportsByDefault keeps unqualified `import Foo` from re-exporting Foo's
        # types through the module's public interface. WebGPU/PAL want this; WebKit has
        # public APIs whose signatures use Foundation/UIKit types via plain `import`,
        # so callers can opt out by setting ${_target}_SWIFT_NO_INTERNAL_IMPORTS_BY_DEFAULT.
        if (NOT ${_target}_SWIFT_NO_INTERNAL_IMPORTS_BY_DEFAULT)
            list(APPEND _swift_options "-enable-upcoming-feature" "InternalImportsByDefault")
        endif ()
        # On non-Apple platforms, Swift's embedded clang doesn't automatically search
        # the compiler's C++ standard library headers (e.g. <coroutine> lives in /usr/include/c++/15/).
        # Pass them explicitly so the wtf umbrella module can include them.
        # Exclude GCC's architecture-specific lib directory (e.g. /usr/lib/gcc/aarch64-linux-gnu/15/include):
        # it contains GCC-specific intrinsic headers (arm_neon.h, etc.) that use GCC builtins
        # unknown to Swift's embedded Clang. Clang provides its own compatible versions in its
        # resource directory and will find them automatically without an explicit -I path.
        # Also exclude libstdc++ version-specific dirs (/usr/include/c++/N,
        # /usr/include/<arch>/c++/N, /usr/include/c++/N/backward) when those come from
        # CMAKE_CXX (which may be GCC of a different version than Swift's embedded clang
        # uses). Pinning the cmake-detected libstdc++ version forces both copies into
        # Swift's clang importer and triggers "different definitions in different modules"
        # errors for std::* types. Swift's clang finds its own libstdc++ on Linux.
        # Likewise exclude the host clang's resource directory (e.g.
        # /usr/lib/llvm-18/lib/clang/18/include): it ships its own builtin
        # module.modulemap, and Swift's embedded clang already loads the one from
        # /opt/swift/usr/lib/swift/clang/include. Forwarding both makes every
        # _Builtin_* / opencl_c module get defined twice and aborts -emit-module.
        if (NOT APPLE)
            foreach (_dir IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
                if (_dir MATCHES "^/usr/lib/gcc/")
                    continue ()
                endif ()
                if (_dir MATCHES "/c\\+\\+/[0-9]+(/|$)")
                    continue ()
                endif ()
                if (_dir MATCHES "/clang/[0-9]+/include(/|$)")
                    continue ()
                endif ()
                list(APPEND _swift_options "-Xcc" "-I${_dir}")
            endforeach ()
        endif ()
        # The clang importer must agree with C++ TUs on every layout-affecting
        # feature check; sanitizers gate ASAN_ENABLED → ENABLE_SECURITY_ASSERTIONS
        # → RefCountDebuggerImpl members. Without this, Swift's inline `new` of a
        # RefCounted C++ type undersizes the allocation and the C++ ctor overflows
        # it. -sanitize= instruments Swift codegen; the importer ignores -Xcc
        # -fsanitize= for __has_feature(), so define __SANITIZE_*__ directly so
        # Compiler.h's #ifdef path sets ASAN_ENABLED/TSAN_ENABLED.
        foreach (_sanitizer IN LISTS ENABLE_SANITIZERS)
            list(APPEND _swift_options "-sanitize=${_sanitizer}")
            if (_sanitizer STREQUAL "address")
                list(APPEND _swift_options "-Xcc" "-D__SANITIZE_ADDRESS__")
            elseif (_sanitizer STREQUAL "thread")
                list(APPEND _swift_options "-Xcc" "-D__SANITIZE_THREAD__")
            endif ()
        endforeach ()
        # swiftc spawns swift-plugin-server under sandbox-exec to expand macros
        # (e.g. SwiftUI @State). When the cmake build itself runs inside an
        # outer sandbox that disallows nested sandbox_apply, macro expansion
        # fails with "external macro implementation type ... could not be
        # found". -disable-sandbox skips the inner sandbox; the macros are
        # WebKit's own, so the isolation it provides isn't load-bearing here.
        list(APPEND _swift_options "-disable-sandbox")
        # Explicit module builds (EMB) pre-build all Clang PCMs once via
        # libSwiftScan, so each swift-frontend process reuses them rather than
        # rebuilding from scratch. This is faster when many Swift source files
        # import the same C++ interop modules. Targets opt in by setting
        # ${_target}_SWIFT_EXPLICIT_MODULE_BUILD before the macro call.
        # All Apple targets (PAL/WebGPU/WebKit on iOS and Mac) use EMB;
        # non-Apple builds rely on -module-cache-path for implicit sharing.
        if (${_target}_SWIFT_EXPLICIT_MODULE_BUILD)
            list(APPEND _swift_options "-explicit-module-build")
            # Force experimental clang attributes ON to match cached SwiftShims
            # PCM content. SDK swiftinterfaces carry -strict-memory-safety in
            # their swift-module-flags-ignorable, which on older swift-driver
            # paths implicitly enables late-parse-attributes and
            # bounds-safety-attributes when building SwiftShims PCM. Without
            # the explicit flags here, our consumer's clang has them OFF and
            # rejects the cached PCM with
            #   "experimental late parsing of attributes was enabled in
            #    precompiled file but is currently disabled" (or the
            #    equivalent for bounds-safety).
            # https://bugs.webkit.org/show_bug.cgi?id=312083
            list(APPEND _swift_options
                "-Xcc" "-fexperimental-bounds-safety-attributes"
                "-Xcc" "-fexperimental-late-parse-attributes"
            )
        endif ()
        list(APPEND _swift_options "-module-cache-path" "${CMAKE_BINARY_DIR}/SwiftModuleCache")
        set_property(DIRECTORY "${CMAKE_BINARY_DIR}" APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/SwiftModuleCache")
        list(APPEND _swift_options "-track-system-dependencies")
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
                # Our just-built frameworks must come before the SDK's
                # PrivateFrameworks so `<WebCore/X.h>` etc. resolve to cmake-built
                # copies. Mirrors Xcode's BUILT_PRODUCTS_DIR precedence over SDK.
                "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}>"
                "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-F ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}>"
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

        # Empty list means: skip Swift C++ interop header generation entirely.
        # Useful for iOS where some Swift files transitively import broken
        # umbrella modules. https://bugs.webkit.org/show_bug.cgi?id=312083
        set(_skip_swift_cxx_header FALSE)
        if (DEFINED ${_target}_SWIFT_TYPECHECK_SOURCES AND "${${_target}_SWIFT_TYPECHECK_SOURCES}" STREQUAL "")
            set(_skip_swift_cxx_header TRUE)
        endif ()

        cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR include OUTPUT_VARIABLE _header_base_path)
        cmake_path(APPEND _header_base_path ${_output_header} OUTPUT_VARIABLE _header_path)

        cmake_path(GET _header_path FILENAME _header_filename)
        cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR "${_header_filename}.tmp" OUTPUT_VARIABLE _header_tmp_path)
        set(_header_stamp_path "${_header_path}.stamp")
        if (NOT DEFINED ${_target}_SWIFT_EMIT_CLANG_HEADER_MIN_ACCESS)
            set(${_target}_SWIFT_EMIT_CLANG_HEADER_MIN_ACCESS internal)
        endif ()
        # -emit-clang-header-min-access was added in Swift 6.3. Please simplify
        # this once that becomes mandatory.
        if (CMAKE_Swift_COMPILER_VERSION VERSION_GREATER_EQUAL 6.3)
            set(CAN_USE_EMIT_CLANG_HEADER_MIN_ACCESS TRUE)
        endif ()
        # Always create the SwiftCxxHeader placeholder so external callers
        # (e.g. Source/WebKit/CMakeLists.txt's deferred add_dependencies) can
        # reference it even when ${_target}_SWIFT_TYPECHECK_SOURCES is empty
        # and we skip the C++-interop header emission entirely.
        add_custom_target(${_target}_SwiftCxxHeader)
        # Placeholder ordering target. The deferred _webkit_setup_swift_header_deps
        # call populates it with all generated (binary-dir) headers for this target
        # once ${_target}_HEADERS and ${_target}_DERIVED_SOURCES are fully known.
        add_custom_target(${_target}_SwiftGeneratedDeps)
        # Generate the @-response file with platform-derived -D flags. The
        # @-flag itself is already in _swift_options above, which gets fed to
        # target_compile_options via _swift_only_options earlier in the macro.
        _webkit_generate_platform_swift_args(${_target} "${_resp_path}" ${_target}_SwiftGeneratedDeps)

        # Trigger source whose mtime tracks the resp (and the emit-clang-header
        # stamp / INTEROP_HEADERS when applicable). target_sources is how we
        # plumb file-level deps in since CMake's Swift compile ignores
        # OBJECT_DEPENDS. Created eagerly so the resp's add_custom_command
        # has an in-graph consumer even when the typecheck path is skipped.
        set(_trigger_path "${CMAKE_CURRENT_BINARY_DIR}/${_target}_SwiftRebuildTrigger.swift")
        if (NOT EXISTS "${_trigger_path}")
            file(WRITE "${_trigger_path}" "// Auto-generated; mtime tracks ${_target}'s Swift inputs.\n")
        endif ()
        set(_trigger_deps "${_resp_path}")
        if (DEFINED ${_target}_SWIFT_INTEROP_SOURCES)
            list(APPEND _trigger_deps ${${_target}_SWIFT_INTEROP_HEADERS})
        elseif (NOT _skip_swift_cxx_header)
            list(APPEND _trigger_deps "${_header_stamp_path}")
        endif ()
        add_custom_command(
            OUTPUT "${_trigger_path}"
            DEPENDS ${_trigger_deps}
            COMMAND ${CMAKE_COMMAND} -E touch "${_trigger_path}"
            COMMENT "Refreshing ${_target} Swift rebuild trigger"
        )
        target_sources(${_target} PRIVATE "${_trigger_path}")
        add_custom_target(${_target}_SwiftRebuildTrigger DEPENDS "${_trigger_path}")

        if (NOT _skip_swift_cxx_header)
            # WebKit-Swift-Generated.h.tmp must be written by exactly one target: the one
            # whose ${_module_name}.swiftmodule the copy command below depends on. A
            # second writer races the copy and fails it sporadically.
            # https://bugs.webkit.org/show_bug.cgi?id=316000
            set(_swift_emit_header_flags
                "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-emit-clang-header-path ${_header_tmp_path}>")
            if (CAN_USE_EMIT_CLANG_HEADER_MIN_ACCESS)
                list(APPEND _swift_emit_header_flags
                    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xfrontend -emit-clang-header-min-access -Xfrontend ${${_target}_SWIFT_EMIT_CLANG_HEADER_MIN_ACCESS}>")
            endif ()
            if (POLICY CMP0157)
                # New Swift support: ${_target} is the only Swift compile, so emit here.
                target_compile_options(${_target} PRIVATE ${_swift_emit_header_flags})
            else ()
                # Legacy support compiles the Swift sources twice: in the throw-away
                # ${_target}_SwiftCompile helper (which produces the swiftmodule the
                # copy depends on) and again in ${_target}'s link rule. Emit only from
                # the helper; emitting from ${_target} too would make its link rule
                # write the same temp unordered against the copy and race it. Stashed
                # here, applied to the helper by _webkit_setup_swift_header_deps.
                set(${_target}_SWIFT_EMIT_HEADER_FLAGS "${_swift_emit_header_flags}")
            endif ()
            add_custom_command(
                OUTPUT ${_header_stamp_path}
                BYPRODUCTS ${_header_path}
                DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_module_name}.swiftmodule
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_header_tmp_path} ${_header_path}
                COMMAND ${CMAKE_COMMAND} -E touch ${_header_stamp_path}
                COMMENT "Generating ${_target} Swift-C++ header"
                VERBATIM)

            target_include_directories(${_target} PUBLIC ${_header_base_path})
            # Defer dependency wiring until end-of-directory so if(TARGET ...) inside
            # _webkit_setup_swift_header_deps sees targets declared after this macro
            # call (e.g. ${_target}_CopyHeaders is often created later in the same file).
            cmake_language(DEFER CALL _webkit_setup_swift_header_deps
                "${_target}" "${_header_stamp_path}" "${_header_path}" "${_resp_path}")
        endif ()
    endif ()
endmacro()
