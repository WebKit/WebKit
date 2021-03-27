# Copyright (C) 2021 Igalia S.L.
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
FindLCMS2
---------

Find LCMS2 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``LCMS2::LCMS2``
  The LCMS2 library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``LCMS2_FOUND``
  true if (the requested version of) LCMS2 is available.
``LCMS2_VERSION``
  the version of LCMS2.
``LCMS2_LIBRARIES``
  the libraries to link against to use LCMS2.
``LCMS2_INCLUDE_DIRS``
  where to find the LCMS2 headers.
``LCMS2_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(PC_LCMS2 QUIET lcms2)
    set(LCMS2_COMPILE_OPTIONS ${PC_LCMS2_CFLAGS_OTHER})
    set(LCMS2_VERSION ${PC_LCMS2_VERSION})
endif ()

find_path(LCMS2_INCLUDE_DIR
    NAMES lcms2.h
    HINTS ${PC_LCMS2_INCLUDEDIR} ${PC_LCMS2_INCLUDE_DIRS} ${LCMS2_INCLUDE_DIR}
    PATH_SUFFIXES lcms2 liblcms2
)

find_library(LCMS2_LIBRARY
    NAMES ${LCMS2_NAMES} lcms2 liblcms2 lcms-2 liblcms-2
    HINTS ${PC_LCMS2_LIBDIR} ${PC_LCMS2_LIBRARY_DIRS}
    PATH_SUFFIXES lcms2
)

if (LCMS2_INCLUDE_DIR AND NOT LCMS_VERSION)
    file(READ ${LCMS2_INCLUDE_DIR}/lcms2.h LCMS2_VERSION_CONTENT)
    string(REGEX MATCH "#define[ \t]+LCMS_VERSION[ \t]+([0-9]+)[ \t]*\n" LCMS2_VERSION_MATCH ${LCMS2_VERSION_CONTENT})
    if (LCMS2_VERSION_MATCH)
        string(SUBSTRING ${CMAKE_MATCH_1} 0 1 LCMS2_VERSION_MAJOR)
        string(SUBSTRING ${CMAKE_MATCH_1} 1 2 LCMS2_VERSION_MINOR)
        set(LCMS2_VERSION "${LCMS2_VERSION_MAJOR}.${LCMS2_VERSION_MINOR}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LCMS2
    FOUND_VAR LCMS2_FOUND
    REQUIRED_VARS LCMS2_LIBRARY LCMS2_INCLUDE_DIR
    VERSION_VAR LCMS2_VERSION
)

if (LCMS2_LIBRARY AND NOT TARGET LCMS2::LCMS2)
    add_library(LCMS2::LCMS2 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(LCMS2::LCMS2 PROPERTIES
        IMPORTED_LOCATION "${LCMS2_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${LCMS2_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${LCMS2_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(LCMS2_INCLUDE_DIR LCMS2_LIBRARY)

if (LCMS2_FOUND)
    set(LCMS2_LIBRARIES ${LCMS2_LIBRARY})
    set(LCMS2_INCLUDE_DIRS ${LCMS2_INCLUDE_DIR})
endif ()
