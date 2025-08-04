# Copyright (C) 2025 Igalia S.L.
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
FindDex
-------

Find the ``libdex`` headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Dex::Dex``
  The ``libdex`` library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Dex_FOUND``
  true if (the requested version of) ``libdex`` is available.
``Dex_VERSION``
  version of the found ``libdex`` library.
``Dex_LIBRARIES``
  libraries to link to use ``libdex``.
``Dex_INCLUDE_DIRS``
  where to find the ``libdex`` headers.
``Dex_COMPILE_OPTIONS``
  additional compiler options that should be passed with ``target_compile_options()``
  to targets that use ``libdex``, if the imported target is not used
  for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBDEX libdex-1)
set(Dex_COMPILE_OPTIONS ${PC_LIBDEX_CFLAGS_OTHER})
set(Dex_VERSION ${PC_LIBDEX_VERSION})

find_path(Dex_INCLUDE_DIR
    NAMES libdex.h
    HINTS ${PC_LIBDEX_INCLUDEDIR} ${PC_LIBDEX_INCLUDE_DIRS}
)

find_library(Dex_LIBRARY
    NAMES ${Dex_NAMES} ${PC_LIBDEX_LIBRARIES} libdex-1
    HINTS ${PC_LIBDEX_LIBDIR} ${PC_LIBDEX_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Dex
    FOUND_VAR Dex_FOUND
    REQUIRED_VARS Dex_LIBRARY Dex_INCLUDE_DIR
    VERSION_VAR Dex_VERSION
)

if (Dex_LIBRARY AND NOT TARGET Dex::Dex)
    add_library(Dex::Dex UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Dex::Dex PROPERTIES
        IMPORTED_LOCATION "${Dex_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Dex_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Dex_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Dex_INCLUDE_DIR Dex_LIBRARY)

if (Dex_FOUND)
    set(Dex_LIBRARIES ${Dex_LIBRARY})
    set(Dex_INCLUDE_DIRS ${Dex_INCLUDE_DIR})
endif ()
