# - Try to find libseccomp
# Once done, this will define
#
#  LIBSECCOMP_FOUND - system has libseccomp
#  LIBSECCOMP_INCLUDE_DIRS - the libseccomp include drectories
#  LIBSECCOMP_LIBRARIES - link these to use libseccomp
#
# Copyright (C) 2018 Igalia S.L.
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
pkg_check_modules(PC_LIBSECCOMP libseccomp)

find_path(LIBSECCOMP_INCLUDE_DIRS
    NAMES seccomp.h
    HINTS ${PC_LIBSECCOMP_INCLUDEDIR}
)

find_library(LIBSECCOMP_LIBRARIES
    NAMES seccomp
    HINTS ${PC_LIBSECCOMP_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBSECCOMP
    REQUIRED_VARS LIBSECCOMP_LIBRARIES
    FOUND_VAR LIBSECCOMP_FOUND
    VERSION_VAR PC_LIBSECCOMP_VERSION)

mark_as_advanced(
    LIBSECCOMP_INCLUDE_DIRS
    LIBSECCOMP_LIBRARIES
)