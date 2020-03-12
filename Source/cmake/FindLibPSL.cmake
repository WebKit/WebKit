# Copyright (C) 2018, 2019 Sony Interactive Entertainment Inc.
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
FindLibPSL
--------------

Find LibPSL headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``LibPSL::LibPSL``
  The LibPSL library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``LibPSL_FOUND``
  true if (the requested version of) LibPSL is available.
``LibPSL_VERSION``
  the version of LibPSL.
``LibPSL_LIBRARIES``
  the libraries to link against to use LibPSL.
``LibPSL_INCLUDE_DIRS``
  where to find the LibPSL headers.
``LibPSL_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBPSL QUIET libpsl)
set(LibPSL_COMPILE_OPTIONS ${PC_LIBPSL_CFLAGS_OTHER})
set(LibPSL_VERSION ${PC_LIBPSL_VERSION})

find_path(LibPSL_INCLUDE_DIR
    NAMES libpsl.h
    HINTS ${PC_LIBPSL_INCLUDEDIR} ${PC_LIBPSL_INCLUDE_DIR}
    PATH_SUFFIXES libpsl
)

find_library(LibPSL_LIBRARY
    NAMES ${LibPSL_NAMES} psl
    HINTS ${PC_LIBPSL_LIBDIR} ${PC_LIBPSL_LIBRARY_DIRS}
)

if (LibPSL_INCLUDE_DIR AND NOT LibPSL_VERSION)
    if (EXISTS "${LibPSL_INCLUDE_DIR}/libpsl.h")
        file(READ "${LibPSL_INCLUDE_DIR}/libpsl.h" LIBPSL_VERSION_CONTENT)

        string(REGEX MATCH "#define +PSL_VERSION_MAJOR +([0-9]+)" _dummy "${LIBPSL_VERSION_CONTENT}")
        set(LIBPSL_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +PSL_VERSION_MINOR +([0-9]+)" _dummy "${LIBPSL_VERSION_CONTENT}")
        set(LIBPSL_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +PSL_VERSION_PATCH +([0-9]+)" _dummy "${LIBPSL_VERSION_CONTENT}")
        set(LIBPSL_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(LIBPSL_VERSION "${LIBPSL_VERSION_MAJOR}.${LIBPSL_VERSION_MINOR}.${LIBPSL_VERSION_PATCH}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibPSL
    FOUND_VAR LibPSL_FOUND
    REQUIRED_VARS LibPSL_LIBRARY LibPSL_INCLUDE_DIR
    VERSION_VAR LibPSL_VERSION
)

if (LibPSL_LIBRARY AND NOT TARGET LibPSL::LibPSL)
    add_library(LibPSL::LibPSL UNKNOWN IMPORTED GLOBAL)
    set_target_properties(LibPSL::LibPSL PROPERTIES
        IMPORTED_LOCATION "${LibPSL_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${LibPSL_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibPSL_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(LibPSL_INCLUDE_DIR LIBPSL_LIBRARIES)

if (LibPSL_FOUND)
    set(LibPSL_LIBRARIES ${LibPSL_LIBRARY})
    set(LibPSL_INCLUDE_DIRS ${LibPSL_INCLUDE_DIR})
endif ()
