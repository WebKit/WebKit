# Copyright (C) 2020 Sony Interactive Entertainment Inc.
# Copyright (C) 2017 Igalia S.L.
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
FindWOFF2
--------------

Find WOFF2 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``WOFF2::common``
  The WOFF2 common library, if found.

``WOFF2::dec``
  The WOFF2 dec library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``WOFF2_FOUND``
  true if (the requested version of) WOFF2 is available.
``WOFF2_VERSION``
  the version of WOFF2.
``WOFF2_LIBRARIES``
  the libraries to link against to use WOFF2.
``WOFF2_INCLUDE_DIRS``
  where to find the WOFF2 headers.
``WOFF2_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_WOFF2 QUIET libwoff2common)
set(WOFF2_COMPILE_OPTIONS ${PC_WOFF2_CFLAGS_OTHER})
set(WOFF2_VERSION ${PC_WOFF2_VERSION})

find_library(WOFF2_LIBRARY
    NAMES ${WOFF2_NAMES} woff2common
    HINTS ${PC_WOFF2_LIBDIR} ${PC_WOFF2_LIBRARY_DIRS}
)

# There's nothing in the WOFF2 headers that could be used to detect the exact
# WOFF2 version being used so don't attempt to do so. A version can only be found
# through pkg-config
if ("${WOFF2_FIND_VERSION}" VERSION_GREATER "${WOFF2_VERSION}")
    if (WOFF2_VERSION)
        message(FATAL_ERROR "Required version (" ${WOFF2_FIND_VERSION} ") is higher than found version (" ${WOFF2_VERSION} ")")
    else ()
        message(WARNING "Cannot determine WOFF2 version without pkg-config")
    endif ()
endif ()

# Find components
if (WOFF2_LIBRARY)
    set(_WOFF2_REQUIRED_LIBS_FOUND ON)
    set(WOFF2_LIBS_FOUND "WOFF2 (required): ${WOFF2_LIBRARY}")
else ()
    set(_WOFF2_REQUIRED_LIBS_FOUND OFF)
    set(WOFF2_LIBS_NOT_FOUND "WOFF2 (required)")
endif ()

if ("dec" IN_LIST WOFF2_FIND_COMPONENTS)
    pkg_check_modules(PC_WOFF2_DEC libwoff2dec)

    find_path(WOFF2_INCLUDE_DIR
        NAMES woff2/decode.h
        HINTS ${PC_WOFF2_DEC_INCLUDEDIR} ${PC_WOFF2_DEC_INCLUDE_DIRS}
    )

    find_library(WOFF2_DEC_LIBRARY
        NAMES ${WOFF2_DEC_NAMES} woff2dec
        HINTS ${PC_WOFF2_DEC_LIBDIR} ${PC_WOFF2_DEC_LIBRARY_DIRS}
    )

    if (WOFF2_DEC_LIBRARY)
        if (WOFF2_FIND_REQUIRED_dec)
            list(APPEND WOFF2_LIBS_FOUND "dec (required): ${WOFF2_DEC_LIBRARY}")
        else ()
           list(APPEND WOFF2_LIBS_FOUND "dec (optional): ${WOFF2_DEC_LIBRARY}")
        endif ()
    else ()
        if (WOFF2_FIND_REQUIRED_dec)
           set(_WOFF2_REQUIRED_LIBS_FOUND OFF)
           list(APPEND WOFF2_LIBS_NOT_FOUND "dec (required)")
        else ()
           list(APPEND WOFF2_LIBS_NOT_FOUND "dec (optional)")
        endif ()
    endif ()
endif ()

if (NOT WOFF2_FIND_QUIETLY)
    if (WOFF2_LIBS_FOUND)
        message(STATUS "Found the following WOFF2 libraries:")
        foreach (found ${WOFF2_LIBS_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
    if (WOFF2_LIBS_NOT_FOUND)
        message(STATUS "The following WOFF2 libraries were not found:")
        foreach (found ${WOFF2_LIBS_NOT_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WOFF2
    FOUND_VAR WOFF2_FOUND
    REQUIRED_VARS WOFF2_LIBRARY _WOFF2_REQUIRED_LIBS_FOUND
    VERSION_VAR WOFF2_VERSION
)

if (WOFF2_LIBRARY AND NOT TARGET WOFF2::common)
    add_library(WOFF2::common UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WOFF2::common PROPERTIES
        IMPORTED_LOCATION "${WOFF2_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${WOFF2_COMPILE_OPTIONS}"
    )
endif ()

if (WOFF2_DEC_LIBRARY AND NOT TARGET WOFF2::dec)
    add_library(WOFF2::dec UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WOFF2::dec PROPERTIES
        IMPORTED_LOCATION "${WOFF2_DEC_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${WOFF2_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${WOFF2_DEC_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    WOFF2_LIBRARY
    WOFF2_DEC_INCLUDE_DIR
    WOFF2_DEC_LIBRARY
)

if (WOFF2_FOUND)
    set(WOFF2_LIBRARIES ${WOFF2_LIBRARY} ${WOFF2_DEC_LIBRARY})
    set(WOFF2_INCLUDE_DIRS ${WOFF2_INCLUDE_DIR})
endif ()
