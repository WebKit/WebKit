# - Find GBM
# This module looks for GBM. This module defines the
# following variables:
#  GBM_FOUND - system has gbm
#  GBM_INCLUDE_DIRS - the gbm include directories
#  GBM_LIBRARIES - link these to use gbm

find_package(PkgConfig QUIET)
pkg_check_modules(PC_GBM QUIET gbm)

find_library(GBM_LIBRARIES
    NAMES gbm
    HINTS ${PC_GBM_LIBDIR}
          ${PC_GBM_LIBRARY_DIRS}
)

find_path(GBM_INCLUDE_DIR
    NAMES gbm.h
    HINTS ${PC_GBM_INCLUDEDIR}
          ${PC_GBM_INCLUDE_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GBM DEFAULT_MSG GBM_LIBRARIES GBM_INCLUDE_DIR)

mark_as_advanced(
  GBM_INCLUDE_DIR
  GBM_LIBRARIES
)
