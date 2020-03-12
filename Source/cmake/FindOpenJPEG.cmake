# Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
FindOpenJPEG
--------------

Find OpenJPEG headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``OpenJPEG::OpenJPEG``
  The OpenJPEG library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``OpenJPEG_FOUND``
  true if (the requested version of) OpenJPEG is available.
``OpenJPEG_VERSION``
  the version of OpenJPEG.
``OpenJPEG_LIBRARIES``
  the libraries to link against to use OpenJPEG.
``OpenJPEG_INCLUDE_DIRS``
  where to find the OpenJPEG headers.
``OpenJPEG_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_OPENJPEG QUIET libopenjp2)
set(OpenJPEG_COMPILE_OPTIONS ${PC_OPENJPEG_CFLAGS_OTHER})
set(OpenJPEG_VERSION ${PC_OPENJPEG_VERSION})

find_path(OpenJPEG_INCLUDE_DIR
    NAMES opj_config.h
    HINTS ${PC_OPENJPEG_INCLUDEDIR} ${PC_OPENJPEG_INCLUDE_DIRS}
)

find_library(OpenJPEG_LIBRARY
    NAMES ${OpenJPEG_NAMES} openjp2
    HINTS ${PC_OPENJPEG_LIBDIR} ${PC_OPENJPEG_LIBRARY_DIRS}
)

if (OpenJPEG_INCLUDE_DIR AND NOT OpenJPEG_VERSION)
    if (EXISTS "${OpenJPEG_INCLUDE_DIR}/opj_config.h")
        file(READ "${OpenJPEG_INCLUDE_DIR}/opj_config.h" OpenJPEG_VERSION_CONTENT)

        string(REGEX MATCH "#define +OPJ_VERSION_MAJOR +([0-9]+)" _dummy "${OpenJPEG_VERSION_CONTENT}")
        set(OpenJPEG_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +OPJ_VERSION_MINOR +([0-9]+)" _dummy "${OpenJPEG_VERSION_CONTENT}")
        set(OpenJPEG_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +OPJ_VERSION_BUILD +([0-9]+)" _dummy "${OpenJPEG_VERSION_CONTENT}")
        set(OpenJPEG_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(OpenJPEG_VERSION "${OpenJPEG_VERSION_MAJOR}.${OpenJPEG_VERSION_MINOR}.${OpenJPEG_VERSION_PATCH}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenJPEG
    FOUND_VAR OpenJPEG_FOUND
    REQUIRED_VARS OpenJPEG_LIBRARY OpenJPEG_INCLUDE_DIR
    VERSION_VAR OpenJPEG_VERSION
)

if (OpenJPEG_LIBRARY AND NOT TARGET OpenJPEG::OpenJPEG)
    add_library(OpenJPEG::OpenJPEG UNKNOWN IMPORTED GLOBAL)
    set_target_properties(OpenJPEG::OpenJPEG PROPERTIES
        IMPORTED_LOCATION "${OpenJPEG_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${OpenJPEG_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${OpenJPEG_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(OpenJPEG_INCLUDE_DIR OpenJPEG_LIBRARY)

if (OpenJPEG_FOUND)
    set(OpenJPEG_LIBRARIES ${OpenJPEG_LIBRARY})
    set(OpenJPEG_INCLUDE_DIRS ${OpenJPEG_INCLUDE_DIR})
endif ()
