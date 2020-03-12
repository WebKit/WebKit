# Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
FindFontconfig
--------------

Find Fontconfig headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Fontconfig::Fontconfig``
  The Fontconfig library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Fontconfig_FOUND``
  true if (the requested version of) Fontconfig is available.
``Fontconfig_VERSION``
  the version of Fontconfig.
``Fontconfig_LIBRARIES``
  the libraries to link against to use Fontconfig.
``Fontconfig_INCLUDE_DIRS``
  where to find the Fontconfig headers.
``Fontconfig_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

# TODO: Remove when cmake_minimum_version bumped to 3.14.

find_package(PkgConfig QUIET)
pkg_check_modules(PC_FONTCONFIG QUIET fontconfig)
set(Fontconfig_COMPILE_OPTIONS ${PC_FONTCONFIG_CFLAGS_OTHER})
set(Fontconfig_VERSION ${PC_FONTCONFIG_CFLAGS_VERSION})

find_path(Fontconfig_INCLUDE_DIR
    NAMES fontconfig/fontconfig.h
    HINTS ${PC_FONTCONFIG_INCLUDEDIR} ${PC_FONTCONFIG_INCLUDE_DIRS} /usr/X11/include
)

find_library(Fontconfig_LIBRARY
    NAMES ${Fontconfig_NAMES} fontconfig
    HINTS ${PC_FONTCONFIG_LIBDIR} ${PC_FONTCONFIG_LIBRARY_DIRS}
)

# Taken from CMake's FindFontconfig.cmake
if (Fontconfig_INCLUDE_DIR AND NOT Fontconfig_VERSION)
  file(STRINGS ${Fontconfig_INCLUDE_DIR}/fontconfig/fontconfig.h _contents REGEX "^#define[ \t]+FC_[A-Z]+[ \t]+[0-9]+$")
  unset(Fontconfig_VERSION)
  foreach (VPART MAJOR MINOR REVISION)
    foreach (VLINE ${_contents})
      if (VLINE MATCHES "^#define[\t ]+FC_${VPART}[\t ]+([0-9]+)$")
        set(Fontconfig_VERSION_PART "${CMAKE_MATCH_1}")
        if (Fontconfig_VERSION)
          string(APPEND Fontconfig_VERSION ".${Fontconfig_VERSION_PART}")
        else ()
          set(Fontconfig_VERSION "${Fontconfig_VERSION_PART}")
        endif ()
      endif ()
    endforeach ()
  endforeach ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Fontconfig
    FOUND_VAR Fontconfig_FOUND
    REQUIRED_VARS Fontconfig_LIBRARY Fontconfig_INCLUDE_DIR
    VERSION_VAR Fontconfig_VERSION
)

if (Fontconfig_LIBRARY AND NOT TARGET Fontconfig::Fontconfig)
    add_library(Fontconfig::Fontconfig UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Fontconfig::Fontconfig PROPERTIES
        IMPORTED_LOCATION "${Fontconfig_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Fontconfig_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Fontconfig_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Fontconfig_INCLUDE_DIR Fontconfig_LIBRARIES)

if (Fontconfig_FOUND)
    set(Fontconfig_LIBRARIES ${Fontconfig_LIBRARY})
    set(Fontconfig_INCLUDE_DIRS ${Fontconfig_INCLUDE_DIR})
endif ()
