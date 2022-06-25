# - Find libdrm
# This module looks for libdrm. This module defines the
# following variables:
#  LIBDRM_FOUND - system has libdrm
#  LIBDRM_INCLUDE_DIRS - the libdrm include directories
#  LIBDRM_LIBRARIES - link these to use libdrm

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBDRM QUIET libdrm)

find_library(LIBDRM_LIBRARIES
    NAMES drm
    HINTS ${PC_LIBDRM_LIBDIR}
          ${PC_LIBDRM_LIBRARY_DIRS}
)

find_path(LIBDRM_INCLUDE_DIR
    NAMES drm.h
    HINTS ${PC_LIBDRM_INCLUDEDIR}
          ${PC_LIBDRM_INCLUDE_DIRS}
    PATH_SUFFIXES libdrm
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibDRM DEFAULT_MSG LIBDRM_LIBRARIES LIBDRM_INCLUDE_DIR)

mark_as_advanced(
  LIBDRM_INCLUDE_DIR
  LIBDRM_LIBRARIES
)
