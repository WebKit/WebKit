# Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
FindBrotli
--------------

Find Brotli headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Brotli::common``
  The Brotli common library, if found.

``Brotli::dec``
  The Brotli dec library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Brotli_FOUND``
  true if (the requested version of) Brotli is available.
``Brotli_VERSION``
  the version of Brotli.
``Brotli_LIBRARIES``
  the libraries to link against to use Brotli.
``Brotli_INCLUDE_DIRS``
  where to find the Brotli headers.
``Brotli_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Brotli QUIET libbrotlicommon)
set(Brotli_COMPILE_OPTIONS ${PC_Brotli_CFLAGS_OTHER})
set(Brotli_VERSION ${PC_Brotli_VERSION})

find_library(Brotli_LIBRARY
    NAMES ${Brotli_NAMES} brotlicommon
    HINTS ${PC_Brotli_LIBDIR} ${PC_Brotli_LIBRARY_DIRS}
)

# There's nothing in the Brotli headers that could be used to detect the exact
# Brotli version being used so don't attempt to do so. A version can only be found
# through pkg-config
if ("${Brotli_FIND_VERSION}" VERSION_GREATER "${Brotli_VERSION}")
    if (Brotli_VERSION)
        message(FATAL_ERROR "Required version (" ${Brotli_FIND_VERSION} ") is higher than found version (" ${Brotli_VERSION} ")")
    else ()
        message(WARNING "Cannot determine Brotli version without pkg-config")
    endif ()
endif ()

# Find components
if (Brotli_LIBRARY)
    set(_Brotli_REQUIRED_LIBS_FOUND ON)
    set(Brotli_LIBS_FOUND "Brotli (required): ${Brotli_LIBRARY}")
else ()
    set(_Brotli_REQUIRED_LIBS_FOUND OFF)
    set(Brotli_LIBS_NOT_FOUND "Brotli (required)")
endif ()

if ("dec" IN_LIST Brotli_FIND_COMPONENTS)
    pkg_check_modules(PC_Brotli_DEC QUIET brotlidec)

    find_path(Brotli_INCLUDE_DIR
        NAMES brotli/decode.h
        HINTS ${PC_Brotli_DEC_INCLUDEDIR} ${PC_Brotli_DEC_INCLUDE_DIRS}
    )

    find_library(Brotli_DEC_LIBRARY
        NAMES ${Brotli_DEC_NAMES} Brotlidec
        HINTS ${PC_Brotli_DEC_LIBDIR} ${PC_Brotli_DEC_LIBRARY_DIRS}
    )

    if (Brotli_DEC_LIBRARY)
        if (Brotli_FIND_REQUIRED_dec)
            list(APPEND Brotli_LIBS_FOUND "dec (required): ${Brotli_DEC_LIBRARY}")
        else ()
            list(APPEND Brotli_LIBS_FOUND "dec (optional): ${Brotli_DEC_LIBRARY}")
        endif ()
    else ()
        if (Brotli_FIND_REQUIRED_dec)
            set(_Brotli_REQUIRED_LIBS_FOUND OFF)
            list(APPEND Brotli_LIBS_NOT_FOUND "dec (required)")
        else ()
            list(APPEND Brotli_LIBS_NOT_FOUND "dec (optional)")
        endif ()
    endif ()
endif ()

if (NOT Brotli_FIND_QUIETLY)
    if (Brotli_LIBS_FOUND)
        message(STATUS "Found the following Brotli libraries:")
        foreach (found ${Brotli_LIBS_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
    if (Brotli_LIBS_NOT_FOUND)
        message(STATUS "The following Brotli libraries were not found:")
        foreach (found ${Brotli_LIBS_NOT_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Brotli
    FOUND_VAR Brotli_FOUND
    REQUIRED_VARS Brotli_LIBRARY _Brotli_REQUIRED_LIBS_FOUND
    VERSION_VAR Brotli_VERSION
)

if (Brotli_LIBRARY AND NOT TARGET Brotli::common)
    add_library(Brotli::common UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Brotli::common PROPERTIES
        IMPORTED_LOCATION "${Brotli_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Brotli_COMPILE_OPTIONS}"
    )
endif ()

if (Brotli_DEC_LIBRARY AND NOT TARGET Brotli::dec)
    add_library(Brotli::dec UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Brotli::dec PROPERTIES
        IMPORTED_LOCATION "${Brotli_DEC_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Brotli_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Brotli_DEC_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    Brotli_LIBRARY
    Brotli_DEC_INCLUDE_DIR
    Brotli_DEC_LIBRARY
)

if (Brotli_FOUND)
    set(Brotli_LIBRARIES ${Brotli_LIBRARY} ${Brotli_DEC_LIBRARY})
    set(Brotli_INCLUDE_DIRS ${Brotli_INCLUDE_DIR})
endif ()
