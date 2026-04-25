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
FindSysProfCapture
------------------

Find Sysprof's ``libsysprof-capture`` headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``SysProfCapture::SysProfCapture``
  The ``libsysprof-capture`` library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``SysProfCapture_FOUND``
  true if (the requested version of) ``libsysprof-capture`` is available.
``SysProfCapture_VERSION``
  version of the found ``libsysprof-capture`` library.
``SysProfCapture_LIBRARIES``
  libraries to link to use ``libsysprof-capture``.
``SysProfCapture_INCLUDE_DIRS``
  where to find the ``libsysprof-capture`` headers.
``SysProfCapture_COMPILE_OPTIONS``
  additional compiler options that should be passed with ``target_compile_options()``
  to targets that use ``libsysprof-capture``, if the imported target is not used
  for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_SYSPROF_CAPTURE sysprof-capture-4)
set(SysProfCapture_COMPILE_OPTIONS ${PC_SYSPROF_CAPTURE_CFLAGS_OTHER})
set(SysProfCapture_VERSION ${PC_SYSPROF_CAPTURE_VERSION})

find_path(SysProfCapture_INCLUDE_DIR
    NAMES sysprof-capture.h
    HINTS ${PC_SYSPROF_CAPTURE_INCLUDEDIR} ${PC_SYSPROF_CAPTURE_INCLUDE_DIR}
    PATH_SUFFIXES sysprof-6 sysprof-4
)

find_library(SysProfCapture_LIBRARY
    NAMES ${SysProfCapture_NAMES} sysprof-capture-4
    HINTS ${PC_SYSPROF_CAPTURE_LIBDIR} ${PC_SYSPROF_CAPTURE_LIBRARY_DIRS}
)

if (SysProfCapture_INCLUDE_DIR AND NOT SysProfCapture_VERSION)
    if (EXISTS "${SysProfCapture_INCLUDE_DIR}/sysprof-version.h")
        file(READ "${SysProfCapture_INCLUDE_DIR}/sysprof-version.h" SysProfCapture_VERSION_CONTENT)

        string(REGEX MATCH "#define[ \t]+SYSPROF_MAJOR_VERSION[ \t]+\(([0-9]+)\)" _dummy "${SysProfCapture_VERSION_CONTENT}")
        set(SysProfCapture_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define[ \t]+SYSPROF_MINOR_VERSION[ \t]+\(([0-9]+)\)" _dummy "${SysProfCapture_VERSION_CONTENT}")
        set(SysProfCapture_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define[ \t]+SYSPROF_MICRO_VERSION[ \t]+\(([0-9]+)\)" _dummy "${SysProfCapture_VERSION_CONTENT}")
        set(SysProfCapture_VERSION_MICRO "${CMAKE_MATCH_1}")

        set(SysProfCapture_VERSION "${SysProfCapture_VERSION_MAJOR}.${SysProfCapture_VERSION_MINOR}.${SysProfCapture_VERSION_MICRO}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SysProfCapture
    FOUND_VAR SysProfCapture_FOUND
    REQUIRED_VARS SysProfCapture_LIBRARY SysProfCapture_INCLUDE_DIR
    VERSION_VAR SysProfCapture_VERSION
)

if (SysProfCapture_LIBRARY AND NOT TARGET SysProfCapture::SysProfCapture)
    add_library(SysProfCapture::SysProfCapture UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SysProfCapture::SysProfCapture PROPERTIES
        IMPORTED_LOCATION "${SysProfCapture_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${SysProfCapture_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${SysProfCapture_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(SysProfCapture_INCLUDE_DIR SysProfCapture_LIBRARY)

if (SysProfCapture_FOUND)
    set(SysProfCapture_LIBRARIES ${SysProfCapture_LIBRARY})
    set(SysProfCapture_INCLUDE_DIRS ${SysProfCapture_INCLUDE_DIR})
endif ()
