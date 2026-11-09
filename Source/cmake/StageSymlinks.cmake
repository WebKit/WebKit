# Usage: cmake -P StageSymlinks.cmake <manifest>
#
# <manifest> has one "<source>|<destination>" pair per line. Point every
# <destination> at its <source> with a symbolic link, leaving links that are
# already correct untouched. WEBKIT_COPY_FILES and WEBKIT_SYMLINK_FILES stage
# the headers of a target with one run of this script instead of one process
# per header.

file(STRINGS "${CMAKE_ARGV3}" _pairs)
set(_made_directories "")
foreach (_pair IN LISTS _pairs)
    string(FIND "${_pair}" "|" _separator)
    string(SUBSTRING "${_pair}" 0 ${_separator} _src)
    math(EXPR _separator "${_separator} + 1")
    string(SUBSTRING "${_pair}" ${_separator} -1 _dst)
    if (NOT EXISTS "${_src}")
        message(FATAL_ERROR "${_src} does not exist; staging it at ${_dst} would leave a dangling symlink.")
    endif ()
    if (IS_SYMLINK "${_dst}")
        file(READ_SYMLINK "${_dst}" _existing)
        if (_existing STREQUAL _src)
            continue ()
        endif ()
    endif ()
    get_filename_component(_dir "${_dst}" DIRECTORY)
    if (NOT _dir IN_LIST _made_directories)
        file(MAKE_DIRECTORY "${_dir}")
        list(APPEND _made_directories "${_dir}")
    endif ()
    file(REMOVE "${_dst}")
    file(CREATE_LINK "${_src}" "${_dst}" SYMBOLIC)
endforeach ()
