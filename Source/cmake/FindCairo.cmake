# Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
FindCairo
--------------

Find Cairo headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Cairo::Cairo``
  The Cairo library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Cairo_FOUND``
  true if (the requested version of) Cairo is available.
``Cairo_VERSION``
  the version of Cairo.
``Cairo_LIBRARIES``
  the libraries to link against to use Cairo.
``Cairo_INCLUDE_DIRS``
  where to find the Cairo headers.
``Cairo_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_CAIRO QUIET cairo)
set(Cairo_COMPILE_OPTIONS ${PC_CAIRO_CFLAGS_OTHER})
set(Cairo_VERSION ${PC_CAIRO_VERSION})

find_path(Cairo_INCLUDE_DIR
    NAMES cairo.h
    HINTS ${PC_CAIRO_INCLUDEDIR} ${PC_CAIRO_INCLUDE_DIR}
    PATH_SUFFIXES cairo
)

find_library(Cairo_LIBRARY
    NAMES ${Cairo_NAMES} cairo
    HINTS ${PC_CAIRO_LIBDIR} ${PC_CAIRO_LIBRARY_DIRS}
)

if (Cairo_INCLUDE_DIR AND NOT Cairo_VERSION)
    if (EXISTS "${Cairo_INCLUDE_DIR}/cairo-version.h")
        file(READ "${Cairo_INCLUDE_DIR}/cairo-version.h" Cairo_VERSION_CONTENT)

        string(REGEX MATCH "#define +CAIRO_VERSION_MAJOR +([0-9]+)" _dummy "${Cairo_VERSION_CONTENT}")
        set(Cairo_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +CAIRO_VERSION_MINOR +([0-9]+)" _dummy "${Cairo_VERSION_CONTENT}")
        set(Cairo_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +CAIRO_VERSION_MICRO +([0-9]+)" _dummy "${Cairo_VERSION_CONTENT}")
        set(Cairo_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(Cairo_VERSION "${Cairo_VERSION_MAJOR}.${Cairo_VERSION_MINOR}.${Cairo_VERSION_PATCH}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cairo
    FOUND_VAR Cairo_FOUND
    REQUIRED_VARS Cairo_LIBRARY Cairo_INCLUDE_DIR
    VERSION_VAR Cairo_VERSION
)

if (Cairo_LIBRARY AND NOT TARGET Cairo::Cairo)
    add_library(Cairo::Cairo UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Cairo::Cairo PROPERTIES
        IMPORTED_LOCATION "${Cairo_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Cairo_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Cairo_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Cairo_INCLUDE_DIR Cairo_LIBRARIES)

if (Cairo_FOUND)
    set(Cairo_LIBRARIES ${Cairo_LIBRARY})
    set(Cairo_INCLUDE_DIRS ${Cairo_INCLUDE_DIR})
endif ()
