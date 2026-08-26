# Copyright (C) 2020 Igalia S.L.
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
FindJournald
-----------

Find Journald-compatible headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Journald::Journald``
  The library where Journald symbols reside, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Journald_FOUND``
  true if (the requested version of) Journald is available.
``Journald_VERSION``
  the version of the library where Journald symbols reside.
``Journald_LIBRARIES``
  the libraries to link against to use Journald.
``Journald_INCLUDE_DIRS``
  where to find the Journald headers.
``Journald_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]
find_package(PkgConfig QUIET)

# libelogind provides compatible pc and header files
pkg_check_modules(PC_SYSTEMD QUIET libsystemd)
if (NOT PC_SYSTEMD_FOUND)
    pkg_check_modules(PC_SYSTEMD QUIET libelogind)
endif ()

set(Journald_COMPILE_OPTIONS ${PC_SYSTEMD_CFLAGS_OTHER})
set(Journald_VERSION ${PC_SYSTEMD_VERSION})

find_path(Journald_INCLUDE_DIR
    NAMES systemd/sd-journal.h
    HINTS ${PC_SYSTEMD_INCLUDEDIR} ${PC_SYSTEMD_INCLUDE_DIRS}
)

find_library(Journald_LIBRARY
    NAMES ${Journald_NAMES} systemd
    HINTS ${PC_SYSTEMD_LIBDIR} ${PC_SYSTEMD_LIBRARY_DIRS}
)

if (NOT Journald_LIBRARY)
    find_library(Journald_LIBRARY
        NAMES ${Journald_NAMES} elogind
        HINTS ${PC_SYSTEMD_LIBDIR} ${PC_SYSTEMD_LIBRARY_DIRS}
    )
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Journald
    FOUND_VAR Journald_FOUND
    REQUIRED_VARS Journald_LIBRARY Journald_INCLUDE_DIR
    VERSION_VAR Journald_VERSION
)

if (Journald_LIBRARY AND NOT TARGET Journald::Journald)
    add_library(Journald::Journald UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Journald::Journald PROPERTIES
        IMPORTED_LOCATION "${Journald_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Journald_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Journald_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Journald_INCLUDE_DIR Journald_LIBRARY)

if (Journald_FOUND)
    set(Journald_LIBRARIES ${Journald_LIBRARY})
    set(Journald_INCLUDE_DIRS ${Journald_INCLUDE_DIR})
endif ()
