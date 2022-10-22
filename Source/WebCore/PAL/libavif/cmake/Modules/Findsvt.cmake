# - Try to find svt_av1
# Once done this will define
#
#  SVT_FOUND - system has svt_av1
#  SVT_INCLUDE_DIR - the svt_av1 include directory
#  SVT_LIBRARIES - Link these to use svt_av1
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
    pkg_check_modules(_SVT SvtAv1Enc)
endif(PKG_CONFIG_FOUND)

find_path(SVT_INCLUDE_DIR NAMES svt-av1/EbSvtAv1Enc.h PATHS ${_SVT_INCLUDEDIR})

find_library(SVT_LIBRARY NAMES SvtAv1Enc PATHS ${_SVT_LIBDIR})

if(SVT_LIBRARY)
    set(SVT_LIBRARIES ${SVT_LIBRARIES} ${SVT_LIBRARY})
endif(SVT_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    svt
    FOUND_VAR SVT_FOUND
    REQUIRED_VARS SVT_LIBRARY SVT_LIBRARIES SVT_INCLUDE_DIR
    VERSION_VAR _SVT_VERSION
)

# show the SVT_INCLUDE_DIR, SVT_LIBRARY and SVT_LIBRARIES variables only
# in the advanced view
mark_as_advanced(SVT_INCLUDE_DIR SVT_LIBRARY SVT_LIBRARIES)
