# - Try to find rav1e
# Once done this will define
#
#  RAV1E_FOUND - system has rav1e
#  RAV1E_INCLUDE_DIR - the rav1e include directory
#  RAV1E_LIBRARIES - Link these to use rav1e
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
    pkg_check_modules(_RAV1E rav1e)
endif(PKG_CONFIG_FOUND)

if(NOT RAV1E_INCLUDE_DIR)
    find_path(
        RAV1E_INCLUDE_DIR
        NAMES rav1e.h
        PATHS ${_RAV1E_INCLUDEDIR}
        PATH_SUFFIXES rav1e
    )
endif()

if(NOT RAV1E_LIBRARY)
    find_library(RAV1E_LIBRARY NAMES rav1e PATHS ${_RAV1E_LIBDIR})
endif()

set(RAV1E_LIBRARIES ${RAV1E_LIBRARIES} ${RAV1E_LIBRARY} ${_RAV1E_LDFLAGS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    rav1e
    FOUND_VAR RAV1E_FOUND
    REQUIRED_VARS RAV1E_LIBRARY RAV1E_LIBRARIES RAV1E_INCLUDE_DIR
    VERSION_VAR _RAV1E_VERSION
)

# show the RAV1E_INCLUDE_DIR, RAV1E_LIBRARY and RAV1E_LIBRARIES variables only
# in the advanced view
mark_as_advanced(RAV1E_INCLUDE_DIR RAV1E_LIBRARY RAV1E_LIBRARIES)
