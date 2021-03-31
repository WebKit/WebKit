# Copyright (C) 2020 Sony Interactive Entertainment Inc.
# Copyright (C) 2021 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

#[=======================================================================[.rst:
FindOpenh264
--------------

Find Openh264 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Openh264::libopenh264``
  The libopenh264 library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Openh264_FOUND``
  true if (the requested version of) Openh264 is available.
``Openh264_VERSION``
  the version of Openh264.
``Openh264_LIBRARIES``
  the libraries to link against to use Openh264.
``Openh264_INCLUDE_DIRS``
  where to find the Openh264 headers.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Openh264 QUIET openh264)
set(Openh264_VERSION ${PC_Openh264_VERSION})

find_path(Openh264_INCLUDE_DIR
    NAMES wels/codec_api.h
    HINTS ${PC_Openh264_INCLUDEDIR} ${PC_Openh264_INCLUDE_DIRS}
)

find_library(Openh264_LIBRARY
    NAMES ${Openh264_NAMES} openh264
    HINTS ${PC_Openh264_LIBDIR} ${PC_Openh264_LIBRARY_DIRS}
)

if (Openh264_INCLUDE_DIR AND NOT Openh264_VERSION)
    if (EXISTS "${Openh264_INCLUDE_DIR}/wels/codec_ver.h")
        file(READ "${Openh264_INCLUDE_DIR}/wels/codec_ver.h" Openh264_VERSION_CONTENT)

        string(REGEX MATCH "#[ \t]*define[ \t]+OPENH264_MAJOR[ \t]+\\(([0-9]+)\\)" _dummy "${Openh264_VERSION_CONTENT}")
        set(Openh264_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#[ \t]*define[ \t]+OPENH264_MINOR[ \t]+\\(([0-9]+)\\)" _dummy "${Openh264_VERSION_CONTENT}")
        set(Openh264_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#[ \t]*define[ \t]+OPENH264_REVISION[ \t]+\\(([0-9]+)\\)" _dummy "${Openh264_VERSION_CONTENT}")
        set(Openh264_VERSION_REVISION "${CMAKE_MATCH_1}")

        set(Openh264_VERSION "${Openh264_VERSION_MAJOR}.${Openh264_VERSION_MINOR}.${Openh264_VERSION_REVISION}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Openh264
    FOUND_VAR Openh264_FOUND
    REQUIRED_VARS Openh264_LIBRARY Openh264_INCLUDE_DIR
    VERSION_VAR Openh264_VERSION
)

if (Openh264_LIBRARY AND NOT TARGET Openh264::libopenh264)
    add_library(Openh264::libopenh264 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Openh264::libopenh264 PROPERTIES
        IMPORTED_LOCATION "${Openh264_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${Openh264_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${Openh264_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Openh264_INCLUDE_DIR Openh264_LIBRARY)

if (Openh264_FOUND)
    set(Openh264_LIBRARIES ${Openh264_LIBRARY})
    set(Openh264_INCLUDE_DIRS ${Openh264_INCLUDE_DIR})
endif ()
