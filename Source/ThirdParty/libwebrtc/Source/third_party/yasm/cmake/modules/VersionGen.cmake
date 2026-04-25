# Redistribution and use is allowed according to the terms of the BSD license.
#
# Copyright (c) 2011 Peter Johnson

macro (VERSION_GEN _version _version_file _default_version)
    set (_vn "v${_default_version}")

    # First see if there is a version file (included in release tarballs),
    # then try git-describe, then default.
    if (EXISTS "${CMAKE_SOURCE_DIR}/version")
        file (STRINGS "${CMAKE_SOURCE_DIR}/version" _version_strs LIMIT_COUNT 1)
        list (GET _version_strs 0 _version_strs0)
        if (NOT (${_version_strs0} STREQUAL ""))
            set (_vn "v${_version_strs0}")
        endif (NOT (${_version_strs0} STREQUAL ""))
    elseif (EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process (COMMAND git describe --match "v[0-9]*" --abbrev=4 HEAD
                         RESULT_VARIABLE _git_result
                         OUTPUT_VARIABLE _git_vn
                         ERROR_QUIET
                         OUTPUT_STRIP_TRAILING_WHITESPACE)

        if (_git_result EQUAL 0)
            # Special handling until we get a more recent tag on the master
            # branch
            if (_git_vn MATCHES "^v0[.]1[.]0")
                execute_process (COMMAND git merge-base v${_default_version} HEAD
                                 OUTPUT_VARIABLE _merge_base
                                 ERROR_QUIET
                                 OUTPUT_STRIP_TRAILING_WHITESPACE)
                #message (STATUS "Merge base: ${_merge_base}")

                execute_process (COMMAND git rev-list ${_merge_base}..HEAD
                                 OUTPUT_VARIABLE _rev_list
                                 ERROR_QUIET)
                string (REGEX MATCHALL "[^\n]*\n" _rev_list_lines "${_rev_list}")
                #message (STATUS "Rev list: ${_rev_list_lines}")
                list (LENGTH _rev_list_lines _vn1)

                execute_process (COMMAND git rev-list --max-count=1 --abbrev-commit --abbrev=4 HEAD
                                 OUTPUT_VARIABLE _vn2
                                 ERROR_QUIET
                                 OUTPUT_STRIP_TRAILING_WHITESPACE)

                set (_git_vn "v${_default_version}-${_vn1}-g${_vn2}")
            endif (_git_vn MATCHES "^v0[.]1[.]0")

            # Append -dirty if there are local changes
            execute_process (COMMAND git update-index -q --refresh)
            execute_process (COMMAND git diff-index --name-only HEAD --
                             OUTPUT_VARIABLE _git_vn_dirty)
            if (_git_vn_dirty)
                set (_git_vn "${_git_vn}-dirty")
            endif (_git_vn_dirty)

            # Substitute . for - in the result
            string (REPLACE "-" "." _vn "${_git_vn}")
        endif (_git_result EQUAL 0)
    endif (EXISTS "${CMAKE_SOURCE_DIR}/version")

    # Strip leading "v" from version
    #message (STATUS "_vn: ${_vn}")
    string (REGEX REPLACE "^v*(.+)" "\\1" _vn "${_vn}")

    # Update version file if required
    if (EXISTS ${_version_file})
        file (STRINGS ${_version_file} _version_strs LIMIT_COUNT 1)
        list (GET _version_strs 0 _vc)
    else (EXISTS ${_version_file})
        set (_vc "unset")
    endif (EXISTS ${_version_file})
    if (NOT ("${_vn}" STREQUAL "${_vc}"))
        file (WRITE ${_version_file} "${_vn}")
    endif (NOT ("${_vn}" STREQUAL "${_vc}"))

    # Set output version variable
    set (${_version} ${_vn})
endmacro (VERSION_GEN)
