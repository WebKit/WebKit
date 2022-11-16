# Copyright (C) 2020 Sony Interactive Entertainment Inc.
# Copyright (C) 2016 Igalia S.L.
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
FindWPE
--------------

Find WPE headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``WPE::libWPE``
  The libWPE library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``WPE_FOUND``
  true if (the requested version of) WPE is available.
``WPE_VERSION``
  the version of WPE.
``WPE_LIBRARIES``
  the libraries to link against to use WPE.
``WPE_INCLUDE_DIRS``
  where to find the WPE headers.
``WPE_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_WPE QUIET wpe-1.0)
set(WPE_COMPILE_OPTIONS ${PC_WPE_CFLAGS_OTHER})
set(WPE_VERSION ${PC_WPE_VERSION})

find_path(WPE_INCLUDE_DIR
    NAMES wpe/wpe.h
    HINTS ${PC_WPE_INCLUDEDIR} ${PC_WPE_INCLUDE_DIRS}
)

find_library(WPE_LIBRARY
    NAMES ${WPE_NAMES} wpe-1.0
    HINTS ${PC_WPE_LIBDIR} ${PC_WPE_LIBRARY_DIRS}
)

if (WPE_INCLUDE_DIR AND NOT WPE_VERSION)
    if (EXISTS "${WPE_INCLUDE_DIR}/wpe/version.h")
        file(READ "${WPE_INCLUDE_DIR}/wpe/version.h" WPE_VERSION_CONTENT)

        string(REGEX MATCH "#define +WPE_MAJOR_VERSION +\\(([0-9]+)\\)" _dummy "${WPE_VERSION_CONTENT}")
        set(WPE_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +WPE_MINOR_VERSION +\\(([0-9]+)\\)" _dummy "${WPE_VERSION_CONTENT}")
        set(WPE_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +WPE_MICRO_VERSION +\\(([0-9]+)\\)" _dummy "${WPE_VERSION_CONTENT}")
        set(WPE_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(WPE_VERSION "${WPE_VERSION_MAJOR}.${WPE_VERSION_MINOR}.${WPE_VERSION_PATCH}")
    endif ()
endif ()

# Versions 1.12.x are the last stable releases where XKB support was always present
if (WPE_VERSION VERSION_LESS 1.13.3)
    list(APPEND WPE_COMPILE_OPTIONS -DWPE_ENABLE_XKB=1)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WPE
    FOUND_VAR WPE_FOUND
    REQUIRED_VARS WPE_LIBRARY WPE_INCLUDE_DIR
    VERSION_VAR WPE_VERSION
)

if (WPE_LIBRARY AND NOT TARGET WPE::libwpe)
    add_library(WPE::libwpe UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WPE::libwpe PROPERTIES
        IMPORTED_LOCATION "${WPE_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${WPE_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${WPE_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(WPE_INCLUDE_DIR WPE_LIBRARY)

if (WPE_FOUND)
    set(WPE_LIBRARIES ${WPE_LIBRARY})
    set(WPE_INCLUDE_DIRS ${WPE_INCLUDE_DIR})
endif ()
