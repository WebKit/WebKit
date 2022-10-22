# - Try to find libsharpyuv
# Once done this will define
#
#  LIBSHARPYUV_FOUND - system has libsharpyuv
#  LIBSHARPYUV_INCLUDE_DIR - the libsharpyuv include directory
#  LIBSHARPYUV_LIBRARIES - Link these to use libsharpyuv
#
#=============================================================================
#  Copyright (c) 2022 Google LLC
#
#  Distributed under the OSI-approved BSD License (the "License");
#  see accompanying file Copyright.txt for details.
#
#  This software is distributed WITHOUT ANY WARRANTY; without even the
#  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the License for more information.
#=============================================================================
#

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(_LIBSHARPYUV libsharpyuv)
endif(PKG_CONFIG_FOUND)

find_path(LIBSHARPYUV_INCLUDE_DIR NAMES sharpyuv/sharpyuv.h PATHS ${_LIBSHARPYUV_INCLUDEDIR})

find_library(LIBSHARPYUV_LIBRARY NAMES sharpyuv PATHS ${_LIBSHARPYUV_LIBDIR})

if(LIBSHARPYUV_LIBRARY)
    set(LIBSHARPYUV_LIBRARIES ${LIBSHARPYUV_LIBRARIES} ${LIBSHARPYUV_LIBRARY})
endif(LIBSHARPYUV_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    libsharpyuv
    FOUND_VAR LIBSHARPYUV_FOUND
    REQUIRED_VARS LIBSHARPYUV_LIBRARY LIBSHARPYUV_LIBRARIES LIBSHARPYUV_INCLUDE_DIR
    VERSION_VAR _LIBSHARPYUV_VERSION
)

# show the LIBSHARPYUV_INCLUDE_DIR, LIBSHARPYUV_LIBRARY and LIBSHARPYUV_LIBRARIES variables
# only in the advanced view
mark_as_advanced(LIBSHARPYUV_INCLUDE_DIR LIBSHARPYUV_LIBRARY LIBSHARPYUV_LIBRARIES)
