# - Try to find BrotliDec.
# Once done, this will define
#
#  BROTLIDEC_FOUND - system has BrotliDec.
#  BROTLIDEC_INCLUDE_DIRS - the BrotliDec include directories
#  BROTLIDEC_LIBRARIES - link these to use BrotliDec.
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
pkg_check_modules(PC_BROTLIDEC libbrotlidec)

find_path(BROTLIDEC_INCLUDE_DIRS
    NAMES brotli/decode.h
    HINTS ${PC_BROTLIDEC_INCLUDEDIR}
)

find_library(BROTLIDEC_LIBRARIES
    NAMES brotlidec
    HINTS ${PC_BROTLIDEC_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BrotliDec
    REQUIRED_VARS BROTLIDEC_INCLUDE_DIRS BROTLIDEC_LIBRARIES
    FOUND_VAR BROTLIDEC_FOUND
    VERSION_VAR PC_BROTLIDEC_VERSION)

mark_as_advanced(
    BROTLIDEC_INCLUDE_DIRS
    BROTLIDEC_LIBRARIES
)
