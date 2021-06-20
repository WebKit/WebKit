# - Try to find LibSoup 2.4
# This module defines the following variables:
#
#  LIBSOUP_FOUND - LibSoup 2.4 was found
#  LIBSOUP_INCLUDE_DIRS - the LibSoup 2.4 include directories
#  LIBSOUP_LIBRARIES - link these to use LibSoup 2.4
#
# Copyright (C) 2012 Raphael Kubo da Costa <rakuco@webkit.org>
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

if (NOT DEFINED LibSoup_FIND_VERSION)
    message(FATAL_ERROR "No LibSoup version specified")
endif ()

if (LibSoup_FIND_VERSION VERSION_LESS 2.91)
    set(LIBSOUP_API_VERSION "2.4")
else ()
    set(LIBSOUP_API_VERSION "3.0")
endif ()

# LibSoup does not provide an easy way to retrieve its version other than its
# .pc file, so we need to rely on PC_LIBSOUP_VERSION and REQUIRE the .pc file
# to be found.
find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBSOUP QUIET "libsoup-${LIBSOUP_API_VERSION}")

find_path(LIBSOUP_INCLUDE_DIRS
    NAMES libsoup/soup.h
    HINTS ${PC_LIBSOUP_INCLUDEDIR}
          ${PC_LIBSOUP_INCLUDE_DIRS}
    PATH_SUFFIXES "libsoup-${LIBSOUP_API_VERSION}"
)

find_library(LIBSOUP_LIBRARIES
    NAMES "soup-${LIBSOUP_API_VERSION}"
    HINTS ${PC_LIBSOUP_LIBDIR}
          ${PC_LIBSOUP_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibSoup REQUIRED_VARS LIBSOUP_INCLUDE_DIRS LIBSOUP_LIBRARIES
                                          VERSION_VAR   PC_LIBSOUP_VERSION)

mark_as_advanced(
    LIBSOUP_INCLUDE_DIRS
    LIBSOUP_LIBRARIES
)
