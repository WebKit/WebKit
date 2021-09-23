# Find LibVPX

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBVPX REQUIRED vpx)

find_path(LIBVPX_INCLUDE_DIRS
    NAMES vpx/vp8.h
    HINTS ${PC_LIBVPX_INCLUDEDIR}
          ${PC_LIBVPX_INCLUDE_DIRS}
)

find_library(LIBVPX_LIBRARIES
    NAMES vpx
    HINTS ${PC_LIBVPX_LIBDIR}
          ${PC_LIBVPX_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibVPX REQUIRED_VARS LIBVPX_INCLUDE_DIRS LIBVPX_LIBRARIES
                                         VERSION_VAR   PC_LIBVPX_VERSION)

mark_as_advanced(
    LIBVPX_INCLUDE_DIRS
    LIBVPX_LIBRARIES
)
