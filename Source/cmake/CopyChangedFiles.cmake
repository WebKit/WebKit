# Usage: cmake -P CopyChangedFiles.cmake <manifest>
#
# <manifest> has one "<source>|<destination>" pair per line. Copy every
# <source> that is newer than its <destination> (or whose destination is
# missing), so that one process can stage a large set of files that would
# otherwise take a command each.

file(STRINGS "${CMAKE_ARGV3}" _pairs)
foreach (_pair IN LISTS _pairs)
    string(FIND "${_pair}" "|" _separator)
    string(SUBSTRING "${_pair}" 0 ${_separator} _src)
    math(EXPR _separator "${_separator} + 1")
    string(SUBSTRING "${_pair}" ${_separator} -1 _dst)
    # IS_NEWER_THAN is also true when the destination is missing.
    if ("${_src}" IS_NEWER_THAN "${_dst}")
        get_filename_component(_dir "${_dst}" DIRECTORY)
        file(MAKE_DIRECTORY "${_dir}")
        file(REMOVE "${_dst}")
        file(COPY_FILE "${_src}" "${_dst}")
    endif ()
endforeach ()
