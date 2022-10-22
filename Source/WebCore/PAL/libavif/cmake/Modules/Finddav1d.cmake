# - Try to find dav1d
# Once done this will define
#
#  DAV1D_FOUND - system has dav1d
#  DAV1D_INCLUDE_DIR - the dav1d include directory
#  DAV1D_LIBRARIES - Link these to use dav1d
#
#=============================================================================
#  Copyright (c) 2020 Andreas Schneider <asn@cryptomilk.org>
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
    pkg_check_modules(_DAV1D dav1d)
endif(PKG_CONFIG_FOUND)

find_path(DAV1D_INCLUDE_DIR NAMES dav1d/dav1d.h PATHS ${_DAV1D_INCLUDEDIR})

find_library(DAV1D_LIBRARY NAMES dav1d PATHS ${_DAV1D_LIBDIR})

if(DAV1D_LIBRARY)
    set(DAV1D_LIBRARIES ${DAV1D_LIBRARIES} ${DAV1D_LIBRARY})
endif(DAV1D_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    dav1d
    FOUND_VAR DAV1D_FOUND
    REQUIRED_VARS DAV1D_LIBRARY DAV1D_LIBRARIES DAV1D_INCLUDE_DIR
    VERSION_VAR _DAV1D_VERSION
)

# show the DAV1D_INCLUDE_DIR, DAV1D_LIBRARY and DAV1D_LIBRARIES variables only
# in the advanced view
mark_as_advanced(DAV1D_INCLUDE_DIR DAV1D_LIBRARY DAV1D_LIBRARIES)
