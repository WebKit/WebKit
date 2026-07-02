# Copyright (C) 2024 Igalia S.L.
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
FindEpoxy
---------

Find the Epoxy (*libepoxy*) headers and library.

Imported Targets
^^^^^^^^^^^^^^^^

``Epoxy::Epoxy``
  The Epoxy library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Epoxy_FOUND``
  true if (the requested version of) Epoxy is available.
``Epoxy_VERSION``
  the version of Epoxy; only defined when a ``pkg-config`` module for
  Epoxy is available.
``Epoxy_LIBRARIES``
  the libraries to link against to use Epoxy.
``Epoxy_INCLUDE_DIRS``
  where to find the Epoxy headers.
``Epoxy_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Epoxy QUIET epoxy)
set(Epoxy_COMPILE_OPTIONS ${PC_Epoxy_CFLAGS_OTHER})
set(Epoxy_VERSION ${PC_Epoxy_VERSION})

find_path(Epoxy_INCLUDE_DIR
    NAMES epoxy/common.h
    HINTS ${PC_Epoxy_INCLUDEDIR} ${PC_Epoxy_INCLUDE_DIR}
)

find_library(Epoxy_LIBRARY
    NAMES ${Epoxy_NAMES} epoxy
    HINTS ${PC_Epoxy_LIBDIR} ${PC_Epoxy_LIBRARY_DIRS}
)

# The libepoxy headers do not have anything usable to detect its version
if ("${Epoxy_FIND_VERSION}" VERSION_GREATER "${Epoxy_VERSION}")
    if (Epoxy_VERSION)
        message(FATAL_ERROR "Required version (${Epoxy_FIND_VERSION}) is"
            " higher than found version (${Epoxy_VERSION})")
    else ()
        message(WARNING "Cannot determine Epoxy version withouit pkg-config")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Epoxy
    FOUND_VAR Epoxy_FOUND
    REQUIRED_VARS Epoxy_LIBRARY
    VERSION_VAR Epoxy_VERSION
)

if (Epoxy_LIBRARY AND NOT TARGET Epoxy::Epoxy)
    add_library(Epoxy::Epoxy UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Epoxy::Epoxy PROPERTIES
        IMPORTED_LOCATION "${Epoxy_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Epoxy_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Epoxy_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    Epoxy_LIBRARY
    Epoxy_INCLUDE_DIR
)

if (Epoxy_FOUND)
    set(Epoxy_LIBRARIES ${Epoxy_LIBRARY})
    set(Epoxy_INCLUDE_DIRS ${Epoxy_INCLUDE_DIR})
endif ()
