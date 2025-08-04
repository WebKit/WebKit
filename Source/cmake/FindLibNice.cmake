# - Try to find LibNice
# Once done, this will define
#
#  NICE_INCLUDE_DIRS - the LibNice include directories
#  NICE_LIBRARIES - link these to use LibNice
#
# Copyright (C) 2025 Igalia S.L
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

pkg_check_modules(PC_NICE nice)

find_path(LibNice_INCLUDE_DIR
    NAMES nice.h
    HINTS ${PC_NICE_INCLUDEDIR}
          ${PC_NICE_INCLUDE_DIRS}
)

find_library(LibNice_LIBRARY
    NAMES nice
    HINTS ${PC_NICE_LIBDIR}
          ${PC_NICE_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibNice
    FOUND_VAR LibNice_FOUND
    REQUIRED_VARS LibNice_LIBRARY LibNice_INCLUDE_DIR
    VERSION_VAR LibNice_VERSION
)

if (LibNice_LIBRARY AND NOT TARGET LibNice::LibNice)
    add_library(LibNice::LibNice UNKNOWN IMPORTED GLOBAL)
    set_target_properties(LibNice::LibNice PROPERTIES
        IMPORTED_LOCATION "${LibNice_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${LibNice_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibNice_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(LibNice_INCLUDE_DIR LibNice_LIBRARY)

if (LibNice_FOUND)
    set(LibNice_LIBRARIES ${LibNice_LIBRARY})
    set(LibNice_INCLUDE_DIRS ${LibNice_INCLUDE_DIR})
endif ()
