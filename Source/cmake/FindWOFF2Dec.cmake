# - Try to find WOFF2Dec.
# Once done, this will define
#
#  WOFF2DEC_FOUND - system has WOFF2Dec.
#  WOFF2DEC_INCLUDE_DIRS - the WOFF2Dec include directories
#  WOFF2DEC_LIBRARIES - link these to use WOFF2Dec.
#
# Copyright (C) 2017 Igalia S.L.
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

find_package(PkgConfig)
pkg_check_modules(PC_WOFF2DEC libwoff2dec)

find_path(WOFF2DEC_INCLUDE_DIRS
    NAMES woff2/decode.h
    HINTS ${PC_WOFF2DEC_INCLUDEDIR}
)

find_library(WOFF2DEC_LIBRARIES
    NAMES woff2dec
    HINTS ${PC_WOFF2DEC_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WOFF2Dec
    REQUIRED_VARS WOFF2DEC_INCLUDE_DIRS WOFF2DEC_LIBRARIES
    FOUND_VAR WOFF2DEC_FOUND
    VERSION_VAR PC_WOFF2DEC_VERSION)

mark_as_advanced(
    WOFF2DEC_INCLUDE_DIRS
    WOFF2DEC_LIBRARIES
)
