# - Try to find WPEBackend-fdo.
# Once done, this will define
#
#  WPEBACKEND_FDO_FOUND - system has WPEBackend-fdo.
#  WPEBACKEND_FDO_INCLUDE_DIRS - the WPEBackend-fdo include directories
#  WPEBACKEND_FDO_LIBRARIES - link these to use WPEBackend-fdo.
#
# Copyright (C) 2016 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig QUIET)
pkg_check_modules(PC_WPEBACKEND_FDO QUIET wpebackend-fdo-1.0)

find_path(WPEBACKEND_FDO_INCLUDE_DIRS
    NAMES wpe/fdo.h
    HINTS ${PC_WPEBACKEND_FDO_INCLUDEDIR} ${PC_WPEBACKEND_FDO_INCLUDE_DIRS}
)

find_library(WPEBACKEND_FDO_LIBRARIES
    NAMES WPEBackend-fdo-1.0
    HINTS ${PC_WPEBACKEND_FDO_LIBDIR} ${PC_WPEBACKEND_FDO_LIBRARY_DIRS}
)

mark_as_advanced(WPEBACKEND_FDO_INCLUDE_DIRS WPEBACKEND_FDO_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WPEBackend_fdo
    REQUIRED_VARS WPEBACKEND_FDO_INCLUDE_DIRS WPEBACKEND_FDO_LIBRARIES
    FOUND_VAR WPEBACKEND_FDO_FOUND
    VERSION_VAR PC_WPEBACKEND_FDO_VERSION)
