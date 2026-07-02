# Usage: cmake -P SymlinkHeaders.cmake <src-dir> <dst-dir>
#
# For every regular file in <src-dir>, create a symlink in <dst-dir> with the
# same basename pointing at the source. This mirrors what Xcode's framework
# Headers/PrivateHeaders alias to: the staged headers under the cmake build's
# own staging dir. Using symlinks (rather than copies) means clang sees the
# same inode whether the header is reached via -I<staging>/WebKit/X.h or via
# -F-framework WebKit/PrivateHeaders/X.h, so it does not emit duplicate-
# definition errors when the modulemap pulls a header in twice.

set(_src "${CMAKE_ARGV3}")
set(_dst "${CMAKE_ARGV4}")
file(MAKE_DIRECTORY "${_dst}")

file(GLOB _entries RELATIVE "${_src}" "${_src}/*")
foreach (_name IN LISTS _entries)
    set(_src_path "${_src}/${_name}")
    set(_dst_path "${_dst}/${_name}")
    if (IS_DIRECTORY "${_src_path}")
        continue ()
    endif ()
    set(_existing "")
    if (IS_SYMLINK "${_dst_path}")
        execute_process(COMMAND readlink "${_dst_path}" OUTPUT_VARIABLE _existing OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif ()
    if (NOT _existing STREQUAL _src_path)
        file(REMOVE "${_dst_path}")
        file(CREATE_LINK "${_src_path}" "${_dst_path}" SYMBOLIC)
    endif ()
endforeach ()
