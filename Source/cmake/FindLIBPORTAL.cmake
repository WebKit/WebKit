# - Try to find libportal
# Once done, this will define
#
#  LIBPORTAL_FOUND - system has libportal
#  LIBPORTAL_INCLUDE_DIRS - the libportal include directories
#  LIBPORTAL_LIBRARIES - link these to use libportal
#
# Copyright (C) 2021 Igalia S.L.
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
FindLIBPORTAL
-------------

Find libportal headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``LIBPORTAL::LIBPORTAL``
  The LIBPORTAL library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``LIBPORTAL_FOUND``
  true if (the requested version of) LIBPORTAL is available.
``LIBPORTAL_VERSION``
  the version of LIBPORTAL.
``LIBPORTAL_LIBRARIES``
  the libraries to link against to use LIBPORTAL.
``LIBPORTAL_INCLUDE_DIRS``
  where to find the LIBPORTAL headers.
``LIBPORTAL_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_LIBPORTAL QUIET libportal)
set(LIBPORTAL_COMPILE_OPTIONS ${PC_LIBPORTAL_CFLAGS_OTHER})
set(LIBPORTAL_VERSION ${PC_LIBPORTAL_VERSION})

find_path(LIBPORTAL_INCLUDE_DIR
    NAMES portal.h
    HINTS ${PC_LIBPORTAL_INCLUDEDIR}
          ${PC_LIBPORTAL_INCLUDE_DIRS}
    PATH_SUFFIXES libportal
)

find_library(LIBPORTAL_LIBRARY
    NAMES ${LIBPORTAL_NAMES} libportal
    HINTS ${PC_LIBPORTAL_LIBDIR}
          ${PC_LIBPORTAL_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBPORTAL
    FOUND_VAR LIBPORTAL_FOUND
    REQUIRED_VARS LIBPORTAL_INCLUDE_DIR LIBPORTAL_LIBRARY
    VERSION_VAR LIBPORTAL_VERSION
)

if (LIBPORTAL_LIBRARY AND NOT TARGET LIBPORTAL::LIBPORTAL)
    add_library(LIBPORTAL::LIBPORTAL UNKNOWN IMPORTED GLOBAL)
    set_target_properties(LIBPORTAL::LIBPORTAL PROPERTIES
        IMPORTED_LOCATION "${LIBPORTAL_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${LIBPORTAL_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBPORTAL_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(LIBPORTAL_INCLUDE_DIR LIBPORTAL_LIBRARY)

if (LIBPORTAL_FOUND)
    set(LIBPORTAL_LIBRARIES ${LIBPORTAL_LIBRARY})
    set(LIBPORTAL_INCLUDE_DIRS ${LIBPORTAL_INCLUDE_DIR})
endif ()
