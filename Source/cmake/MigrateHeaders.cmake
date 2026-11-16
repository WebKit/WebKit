# Usage: cmake -P MigrateHeaders.cmake <pairs-file>
#   <pairs-file> contains one "src|dst" pair per line.
#
# For each pair, write a forwarding stub at <dst> containing
#   #import <WebKitLegacy/<basename>.h>
# This matches Xcode's MigratedHeaders build phase: the SDK iOS
# WebKit.framework/PrivateHeaders/ contains tiny stubs that re-export from
# WebKitLegacy.framework, not copies of the source content. Re-exporting
# avoids duplicate @interface declarations when WebKitLegacy's modulemap is
# loaded transitively. <src> is preserved in the pairs file only as a
# diagnostic marker; it is not copied.

file(STRINGS "${CMAKE_ARGV3}" _pairs)
foreach (_pair IN LISTS _pairs)
    if (NOT _pair)
        continue ()
    endif ()
    string(REPLACE "|" ";" _pair_list "${_pair}")
    list(GET _pair_list 0 _src)
    list(GET _pair_list 1 _dst)
    get_filename_component(_basename "${_src}" NAME)
    set(_stub_content "#import <WebKitLegacy/${_basename}>\n")
    set(_existing "")
    if (EXISTS "${_dst}")
        file(READ "${_dst}" _existing)
    endif ()
    if (NOT _existing STREQUAL _stub_content)
        # Always remove first: if the destination is (or was transiently) a symlink
        # to a staged WebKitLegacy/WebCore header, file(WRITE) would follow the
        # symlink chain back into the source tree and overwrite the real header.
        file(REMOVE "${_dst}")
        file(WRITE "${_dst}" "${_stub_content}")
    endif ()
endforeach ()
