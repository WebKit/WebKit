# Copyright (C) 2026 Igalia S.L.
# Copyright (C) 2012 Samsung Electronics
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
FindATK
-------

Find the ATK headers and library.

Imported Targets
^^^^^^^^^^^^^^^^

``ATK::ATK``
  The ATK library, if found.

Result Variables
^^^^^^^^^^^^^^^^

``ATK_FOUND``
  true if (the requested version of) the ATK library is available.
``ATK_VERSION``
  the version of the ATK library.
``ATK_LIBRARY``
  path to the ATK library.
``ATK_INCLUDE_DIR``
  path where to find the ATK headers.
``ATK_COMPILE_OPTIONS``
  this should be passed to target_compile_options() of targets that use
  the ATK library, if the ATK::ATK target is not used for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_ATK QUIET atk)
set(ATK_COMPILE_OPTIONS ${PC_ATK_CFLAGS_OTHER})
set(ATK_VERSION ${PC_ATK_VERSION})

find_path(ATK_INCLUDE_DIR
    NAMES atk/atk.h
    HINTS ${PC_ATK_INCLUDEDIR}
          ${PC_ATK_INCLUDE_DIRS}
    PATH_SUFFIXES atk-1.0
)

find_library(ATK_LIBRARY
    NAMES atk-1.0
    HINTS ${PC_ATK_LIBDIR}
          ${PC_ATK_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ATK
    FOUND_VAR ATK_FOUND
    REQUIRED_VARS ATK_LIBRARY ATK_INCLUDE_DIR
    VERSION_VAR ATK_VERSION
)

if (ATK_LIBRARY AND NOT TARGET ATK::ATK)
    add_library(ATK::ATK UNKNOWN IMPORTED GLOBAL)
    set_target_properties(ATK::ATK PROPERTIES
        IMPORTED_LOCATION "${ATK_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${ATK_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${ATK_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(ATK_INCLUDE_DIR ATK_LIBRARY)
