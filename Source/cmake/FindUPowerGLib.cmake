# - Try to find libupower-glib.
# Once done, this will define
#
#  UPOWERGLIB_INCLUDE_DIRS - the libtasn1 include directories
#  UPOWERGLIB_LIBRARIES - the libtasn1 libraries.
#
# Copyright (C) 2017 Collabora Ltd.
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

find_package(PkgConfig)
pkg_check_modules(PC_UPOWERGLIB QUIET upower-glib)

find_path(UPOWERGLIB_INCLUDE_DIRS
    NAMES libupower-glib/upower.h
    PATHS ${PC_UPOWERGLIB_INCLUDEDIR} ${PC_UPOWERGLIB_INCLUDE_DIRS}
)

find_library(UPOWERGLIB_LIBRARIES
    NAMES upower-glib
    PATHS ${PC_UPOWERGLIB_LIBDIR} ${PC_UPOWERGLIB_LIBRARY_DIRS}
)

mark_as_advanced(UPOWERGLIB_INCLUDE_DIRS UPOWERGLIB_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UPowerGLib REQUIRED_VARS UPOWERGLIB_INCLUDE_DIRS UPOWERGLIB_LIBRARIES
                                  VERSION_VAR   PC_UPOWERGLIB_VERSION)
