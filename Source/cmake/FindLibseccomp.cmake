# Copyright (C) 2018, 2020 Igalia S.L.
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
FindLibseccomp
--------------

Find the *libseccomp* headers and library.

Imported Targets
^^^^^^^^^^^^^^^^

``Libseccomp::Libseccomp``
  The *libseccomp* library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Libseccomp_FOUND``
  true if (the requested version of) *libseccomp* is available.
``Libseccomp_VERSION``
  the version of *libseccomp*.
``Libseccomp_LIBRARIES``
  the libraries to link against to use *libseccomp*.
``Libseccomp_INCLUDE_DIRS``
  where to find the *libseccomp* headers.
``Libseccomp_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBSECCOMP QUIET libseccomp)
set(Libseccomp_COMPILE_OPTIONS ${PC_LIBSECCOMP_CFLAGS_OTHER})
set(Libseccomp_VERSION ${PC_LIBSECCOMP_VERSION})

find_path(Libseccomp_INCLUDE_DIR
    NAMES seccomp.h
    HINTS ${PC_LIBSECCOMP_INCLUDEDIR}
          ${PC_LIBSECCOMP_INCLUDE_DIRS}
)

find_library(Libseccomp_LIBRARY
    NAMES ${Libseccomp_NAMES} seccomp
    HINTS ${PC_LIBSECCOMP_LIBDIR}
          ${PC_LIBSECCOMP_LIBRARY_DIRS}
)

if (Libseccomp_INCLUDE_DIR AND NOT Libseccomp_VERSION)
    if (EXISTS "${Libseccomp_INCLUDE_DIR}/seccomp.h")
        file(READ "${Libseccomp_INCLUDE_DIR}/seccomp.h" Libseccomp_VERSION_CONTENT)

        string(REGEX MATCH "#define[ \t]+SCMP_VER_MAJOR[ \t]+([0-9]+)" _dummy "${Libseccomp_VERSION_CONTENT}")
        set(Libseccomp_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define[ \t]+SCMP_VER_MINOR[ \t]+([0-9]+)" _dummy "${Libseccomp_VERSION_CONTENT}")
        set(Libseccomp_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define[ \t]+SCMP_VER_MICRO[ \t]+([0-9]+)" _dummy "${Libseccomp_VERSION_CONTENT}")
        set(Libseccomp_VERSION_MICRO "${CMAKE_MATCH_1}")

        set(Libseccomp_VERSION "${Libseccomp_VERSION_MAJOR}.${Libseccomp_VERSION_MINOR}.${Libseccomp_VERSION_MICRO}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libseccomp
    FOUND_VAR Libseccomp_FOUND
    REQUIRED_VARS Libseccomp_LIBRARY Libseccomp_INCLUDE_DIR
    VERSION_VAR Libseccomp_VERSION
)

if (Libseccomp_LIBRARY AND NOT TARGET Libseccomp::Libseccomp)
    add_library(Libseccomp::Libseccomp UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Libseccomp::Libseccomp PROPERTIES
        IMPORTED_LOCATION "${Libseccomp_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Libseccomp_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libseccomp_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Libseccomp_INCLUDE_DIR Libseccomp_LIBRARY)

if (Libseccomp_FOUND)
    set(Libseccomp_LIBRARIES ${Libseccomp_LIBRARY})
    set(Libseccomp_INCLUDE_DIRS ${Libseccomp_INCLUDE_DIR})
endif ()
