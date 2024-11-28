# - Try to find LibSpiel
# Once done, this will define
#
#  SPIEL_INCLUDE_DIRS - the LibSpiel include directories
#  SPIEL_LIBRARIES - link these to use LibSpiel
#
# Copyright (C) 2024 Igalia S.L
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

pkg_check_modules(PC_SPIEL spiel-1.0)

find_path(LibSpiel_INCLUDE_DIR
    NAMES spiel.h
    HINTS ${PC_SPIEL_INCLUDEDIR}
          ${PC_SPIEL_INCLUDE_DIRS}
)

find_library(LibSpiel_LIBRARY
    NAMES spiel-1.0
    HINTS ${PC_SPIEL_LIBDIR}
          ${PC_SPIEL_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibSpiel
    FOUND_VAR LibSpiel_FOUND
    REQUIRED_VARS LibSpiel_LIBRARY LibSpiel_INCLUDE_DIR
    VERSION_VAR LibSpiel_VERSION
)

if (LibSpiel_LIBRARY AND NOT TARGET LibSpiel::LibSpiel)
    add_library(LibSpiel::LibSpiel UNKNOWN IMPORTED GLOBAL)
    set_target_properties(LibSpiel::LibSpiel PROPERTIES
        IMPORTED_LOCATION "${LibSpiel_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${LibSpiel_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibSpiel_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(LibSpiel_INCLUDE_DIR LibSpiel_LIBRARY)

if (LibSpiel_FOUND)
    set(LibSpiel_LIBRARIES ${LibSpiel_LIBRARY})
    set(LibSpiel_INCLUDE_DIRS ${LibSpiel_INCLUDE_DIR})
endif ()
