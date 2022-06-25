# - Try to find libtasn1.
# Once done, this will define
#
#  LIBTASN1_INCLUDE_DIRS - the libtasn1 include directories
#  LIBTASN1_LIBRARIES - the libtasn1 libraries.
#
# Copyright (C) 2017 Metrological Group B.V.
# Copyright (C) 2017 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBTASN1 libtasn1)

find_path(LIBTASN1_INCLUDE_DIRS
    NAMES libtasn1.h
    HINTS ${PC_LIBTASN1_INCLUDEDIR} ${PC_LIBTASN1_INCLUDE_DIRS}
)

find_library(LIBTASN1_LIBRARIES
    NAMES tasn1
    HINTS ${PC_LIBTASN1_LIBDIR} ${PC_LIBTASN1_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libtasn1 REQUIRED_VARS LIBTASN1_LIBRARIES
                                  FOUND_VAR LIBTASN1_FOUND)

mark_as_advanced(LIBTASN1_INCLUDE_DIRS LIBTASN1_LIBRARIES)
