# - Try to find OpenXR.
# Once done, this will define
#
#  OPENXR_FOUND - system has WPE.
#  OPENXR_INCLUDE_DIRS - the WPE include directories
#  OPENXR_LIBRARIES - link these to use WPE.
#
# Copyright (C) 2019 Igalia S.L.
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
pkg_check_modules(PC_OPENXR QUIET openxr)

find_path(OPENXR_INCLUDE_DIRS
    NAMES openxr/openxr.h
    HINTS ${PC_OPENXR_INCLUDEDIR} ${PC_OPENXR_INCLUDE_DIRS}
)

find_library(OPENXR_LIBRARIES
    NAMES openxr_loader
    HINTS ${PC_OPENXR_LIBDIR} ${PC_OPENXR_LIBRARY_DIRS}
)

mark_as_advanced(OPENXR_INCLUDE_DIRS OPENXR_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenXR REQUIRED_VARS OPENXR_INCLUDE_DIRS OPENXR_LIBRARIES)
