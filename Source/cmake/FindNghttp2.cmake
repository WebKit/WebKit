# - Try to find nghttp2
# This module defines the following variables:
#
#  NGHTTP2_FOUND - nghttp2 was found
#  NGHTTP2_INCLUDE_DIRS - the nghttp2 include directories
#  NGHTTP2_LIBRARIES - link these to use nghttp2
#
# Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
pkg_check_modules(PC_NGHTTP2 QUIET nghttp2)

find_path(NGHTTP2_INCLUDE_DIRS
    NAMES nghttp2.h
    HINTS ${PC_NGHTTP2_INCLUDEDIR}
          ${PC_NGHTTP2_INCLUDE_DIRS}
    PATH_SUFFIXES nghttp2
)

find_library(NGHTTP2_LIBRARIES
    NAMES nghttp2
    HINTS ${PC_NGHTTP2_LIBDIR}
          ${PC_NGHTTP2_LIBRARY_DIRS}
)

if (NGHTTP2_INCLUDE_DIRS)
    if (EXISTS "${NGHTTP2_INCLUDE_DIRS}/nghttp2ver.h")
        file(READ "${NGHTTP2_INCLUDE_DIRS}/nghttp2ver.h" _nghttp2_version_content)

        string(REGEX MATCH "#define +NGHTTP2_VERSION +\"([0-9]+\.[0-9]+\.[0-9]+)\"" _dummy "${_nghttp2_version_content}")
        set(NGHTTP2_VERSION "${CMAKE_MATCH_1}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nghttp2 REQUIRED_VARS NGHTTP2_INCLUDE_DIRS NGHTTP2_LIBRARIES
                                          VERSION_VAR NGHTTP2_VERSION)

mark_as_advanced(
    NGHTTP2_INCLUDE_DIRS
    NGHTTP2_LIBRARIES
)
