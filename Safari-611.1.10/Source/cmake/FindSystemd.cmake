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
FindSystemd
-----------

Find Systemd headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Systemd::Systemd``
  The Systemd library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Systemd_FOUND``
  true if (the requested version of) Systemd is available.
``Systemd_VERSION``
  the version of Systemd.
``Systemd_LIBRARIES``
  the libraries to link against to use Systemd.
``Systemd_INCLUDE_DIRS``
  where to find the Systemd headers.
``Systemd_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)

pkg_check_modules(PC_SYSTEMD QUIET libsystemd)
set(Systemd_COMPILE_OPTIONS ${PC_SYSTEMD_CFLAGS_OTHER})
set(Systemd_VERSION ${PC_SYSTEMD_VERSION})

find_path(Systemd_INCLUDE_DIR
    NAMES systemd/sd-journal.h
    HINTS ${PC_SYSTEMD_INCLUDEDIR} ${PC_SYSTEMD_INCLUDE_DIRS}
)

find_library(Systemd_LIBRARY
    NAMES ${Systemd_NAMES} systemd
    HINTS ${PC_SYSTEMD_LIBDIR} ${PC_SYSTEMD_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Systemd
    FOUND_VAR Systemd_FOUND
    REQUIRED_VARS Systemd_LIBRARY Systemd_INCLUDE_DIR
    VERSION_VAR Systemd_VERSION
)

if (Systemd_LIBRARY AND NOT TARGET Systemd::Systemd)
    add_library(Systemd::Systemd UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Systemd::Systemd PROPERTIES
        IMPORTED_LOCATION "${Systemd_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Systemd_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Systemd_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Systemd_INCLUDE_DIR Systemd_LIBRARY)

if (Systemd_FOUND)
    set(Systemd_LIBRARIES ${Systemd_LIBRARY})
    set(Systemd_INCLUDE_DIRS ${Systemd_INCLUDE_DIR})
endif ()
