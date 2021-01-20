# Copyright (C) 2020 Igalia S.L.
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

#[=======================================================================[.rst:
FindManette
-----------

Find Manette headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Manette::Manette``
  The Manette library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Manette_FOUND``
  true if (the requested version of) Manette is available.
``Manette_VERSION``
  the version of Manette.
``Manette_LIBRARIES``
  the libraries to link against to use Manette.
``Manette_INCLUDE_DIRS``
  where to find the Manette headers.
``Manette_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_MANETTE QUIET manette-0.2)
set(Manette_COMPILE_OPTIONS ${PC_MANETTE_CFLAGS_OTHER})
set(Manette_VERSION ${PC_MANETTE_VERSION})

find_path(Manette_INCLUDE_DIR
    NAMES libmanette.h
    HINTS ${PC_MANETTE_INCLUDEDIR}
          ${PC_MANETTE_INCLUDE_DIRS}
)

find_library(Manette_LIBRARY
    NAMES ${Manette_NAMES} manette-0.2
    HINTS ${PC_MANETTE_LIBDIR}
          ${PC_MANETTE_LIBRARY_DIRS}
)

if (Manette_INCLUDE_DIR AND NOT Manette_VERSION)
    if (EXISTS "${Manette_INCLUDE_DIR}/manette-version.h")
        file(READ "${Manette_INCLUDE_DIR}/manette-version.h" Manette_VERSION_CONTENT)

        string(REGEX MATCH "#define +LIBMANETTE_MAJOR_VERSION +([0-9]+)" _dummy "${Manette_VERSION_CONTENT}")
        set(Manette_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +LIBMANETTE_MINOR_VERSION +([0-9]+)" _dummy "${Manette_VERSION_CONTENT}")
        set(Manette_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +LIBMANETTE_MICRO_VERSION +([0-9]+)" _dummy "${Manette_VERSION_CONTENT}")
        set(Manette_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(Manette_VERSION "${Manette_VERSION_MAJOR}.${Manette_VERSION_MINOR}.${Manette_VERSION_PATCH}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Manette
    FOUND_VAR Manette_FOUND
    REQUIRED_VARS Manette_INCLUDE_DIR Manette_LIBRARY
    VERSION_VAR Manette_VERSION
)

if (Manette_LIBRARY AND NOT TARGET Manette::Manette)
    add_library(Manette::Manette UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Manette::Manette PROPERTIES
        IMPORTED_LOCATION "${Manette_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Manette_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Manette_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Manette_INCLUDE_DIR Manette_LIBRARY)

if (Manette_FOUND)
    set(Manette_LIBRARIES ${Manette_LIBRARY})
    set(Manette_INCLUDE_DIRS ${Manette_INCLUDE_DIR})
endif ()
