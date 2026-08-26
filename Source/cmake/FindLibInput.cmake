# Copyright (C) 2023 Igalia S.L.
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
FindLibInput
------------

Find libinput headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``LibInput::LibInput``
  The libinput library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``LibInput_FOUND``
  true if (the requested version of) libinput is available.
``LibInput_VERSION``
  the libinput version.
``LibInput_LIBRARIES``
  the library to link against to use libinput.
``LibInput_INCLUDE_DIRS``
  where to find the libinput headers.
``LibInput_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the imported
  target is not used for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBINPUT QUIET libinput)
set(LibInput_COMPILE_OPTIONS ${PC_LIBINPUT_CFLAGS_OTHER})
set(LibInput_VERSION ${PC_LIBINPUT_VERSION})

find_path(LibInput_INCLUDE_DIR
    NAMES libinput.h
    HINTS ${PC_LIBINPUT_INCLUDEDIR}
          ${PC_LIBINPUT_INCLUDE_DIRS}
)

find_library(LibInput_LIBRARY
    NAMES input
    HINTS ${PC_LIBINPUT_LIBDIR}
          ${PC_LIBINPUT_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibInput
    REQUIRED_VARS LibInput_LIBRARY LibInput_INCLUDE_DIR
    VERSION_VAR LibInput_VERSION
)

if (LibInput_LIBRARY AND NOT TARGET LibInput::LibInput)
    add_library(LibInput::LibInput UNKNOWN IMPORTED GLOBAL)
    set_target_properties(LibInput::LibInput PROPERTIES
        IMPORTED_LOCATION "${LibInput_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${LibInput_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibInput_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    LibInput_INCLUDE_DIR
    LibInput_LIBRARY
)

if (LibInput_FOUND)
    set(LibInput_LIBRARIES ${LibInput_LIBRARY})
    set(LibInput_INCLUDE_DIRS ${LibInput_INCLUDE_DIR})
endif ()
