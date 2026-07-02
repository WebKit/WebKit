# Copyright (C) 2025 Igalia S.L.
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

#[=======================================================================[.rst:
FindSoup3
---------

Find the libsoup 3 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Soup3::Soup3``
  The libsoup 3 library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Soup3_FOUND``
  true if (the requested version of) libsoup 3 is available.
``Soup3_VERSION``
  Version of libsoup 3.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Soup3 QUIET libsoup-3.0)
set(Soup3_COMPILE_OPTIONS ${PC_Soup3_CFLAGS_OTHER})
set(Soup3_VERSION ${PC_Soup3_VERSION})

find_library(Soup3_LIBRARY
    NAMES soup-3.0
    HINTS ${PC_Soup3_LIBDIR}
          ${PC_Soup3_LIBRARY_DIRS}
)

find_path(Soup3_INCLUDE_DIR
    NAMES libsoup/soup.h
    HINTS ${PC_Soup3_INCLUDEDIR}
          ${PC_Soup3_INCLUDE_DIRS}
    PATH_SUFFIXES libsoup-3.0
)

if (Soup3_INCLUDE_DIR AND NOT Soup3_VERSION AND EXISTS "${Soup3_INCLUDE_DIR}/libsoup/soup-version.h")
    file(READ "${Soup3_INCLUDE_DIR}/libsoup/soup-version.h" Soup3_VERSION_CONTENT)

    string(REGEX MATCH "#define[\t ]+SOUP_MAJOR_VERSION[\t ]+\(([0-9]+)\)" _dummy "${Soup3_VERSION_CONTENT}")
    set(Soup3_VERSION_MAJOR "${CMAKE_MATCH_1}")

    string(REGEX MATCH "#define[\t ]+SOUP_MINOR_VERSION[\t ]+\(([0-9]+)\)" _dummy "${Soup3_VERSION_CONTENT}")
    set(Soup3_VERSION_MINOR "${CMAKE_MATCH_1}")

    string(REGEX MATCH "#define[\t ]+SOUP_MICRO_VERSION[\t ]+\(([0-9]+)\)" _dummy "${Soup3_VERSION_CONTENT}")
    set(Soup3_VERSION_MICRO "${CMAKE_MATCH_1}")

    set(Soup3_VERSION "${Soup3_VERSION_MAJOR}.${Soup3_VERSION_MINOR}.${Soup3_VERSION_MICRO}")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Soup3
    REQUIRED_VARS Soup3_LIBRARY Soup3_INCLUDE_DIR
    VERSION_VAR Soup3_VERSION
)

if (Soup3_LIBRARY AND NOT TARGET Soup3::Soup3)
    add_library(Soup3::Soup3 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Soup3::Soup3 PROPERTIES
        IMPORTED_LOCATION "${Soup3_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Soup3_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Soup3_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    Soup3_INCLUDE_DIR
    Soup3_LIBRARY
)
