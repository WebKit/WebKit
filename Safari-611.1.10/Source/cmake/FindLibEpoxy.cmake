# - Try to find libepoxy.
# Once done, this will define
#
#  LIBEPOXY_INCLUDE_DIRS - the libtasn1 include directories
#  LIBEPOXY_LIBRARIES - the libtasn1 libraries.
#
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
pkg_check_modules(PC_LIBEPOXY QUIET epoxy)

if (PC_LIBEPOXY_FOUND)
    set(LIBEPOXY_DEFINITIONS ${PC_LIBEPOXY_CFLAGS_OTHER})
endif ()

find_path(LIBEPOXY_INCLUDE_DIRS
    NAMES epoxy/gl.h
    HINTS ${PC_LIBEPOXY_INCLUDEDIR} ${PC_LIBEPOXY_INCLUDE_DIRS}
)

find_library(LIBEPOXY_LIBRARIES
    NAMES epoxy
    HINTS ${PC_LIBEPOXY_LIBDIR} ${PC_LIBEPOXY_LIBRARY_DIRS}
)

mark_as_advanced(LIBEPOXY_INCLUDE_DIRS LIBEPOXY_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibEpoxy REQUIRED_VARS LIBEPOXY_INCLUDE_DIRS LIBEPOXY_LIBRARIES
                                           VERSION_VAR   PC_LIBEPOXY_VERSION)
