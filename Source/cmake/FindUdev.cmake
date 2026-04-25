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
FindUdev
--------

Find udev headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Udev::Udev``
  The udev library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Udev_FOUND``
  true if (the requested version of) udev is available.
``Udev_VERSION``
  the udev version.
``Udev_LIBRARIES``
  the library to link against to use udev.
``Udev_INCLUDE_DIRS``
  where to find the udev headers.
``Udev_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the imported
  target is not used for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBUDEV QUIET libusev)
set(Udev_COMPILE_OPTIONS ${PC_LIBUDEV_CFLAGS_OTHER})
set(Udev_VERSION ${PC_LIBUDEV_VERSION})

find_path(Udev_INCLUDE_DIR
    NAMES libudev.h
    HINTS ${PC_LIBUDEV_INCLUDEDIR}
          ${PC_LIBUDEV_INCLUDE_DIRS}
)

find_library(Udev_LIBRARY
    NAMES udev
    HINTS ${PC_LIBUDEV_LIBDIR}
          ${PC_LIBUDEV_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Udev
    REQUIRED_VARS Udev_LIBRARY Udev_INCLUDE_DIR
    VERSION_VAR Udev_VERSION
)

if (Udev_LIBRARY AND NOT TARGET Udev::Udev)
    add_library(Udev::Udev UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Udev::Udev PROPERTIES
        IMPORTED_LOCATION "${Udev_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Udev_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Udev_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    Udev_INCLUDE_DIR
    Udev_LIBRARY
)

if (Udev_FOUND)
    set(Udev_LIBRARIES ${Udev_LIBRARY})
    set(Udev_INCLUDE_DIRS ${Udev_INCLUDE_DIR})
endif ()
