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
FindWPEBackendFDO
-----------------

Find the WPEBackend-fdo headers and library.

Imported Targets
^^^^^^^^^^^^^^^^

``WPE::FDO``
  The WPEBackend-fdo library, if found.

Result Variables
^^^^^^^^^^^^^^^^

``WPEBackendFDO_FOUND``
  true if (the requested version of) WPEBackend-fdo is available.
``WPEBackendFDO_VERSION``
  the WPEBackend-fdo versionl.
``WPEBackendFDO_LIBRARIES``
  the libraries to link against to use WPEBackend-fdo.
``WPEBackendFDO_INCLUDE_DIRS``
  where to find the WPEBackend-fdo headers.
``WPEBackendFDO_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the target
  is not used for linking.
``WPEBackendFDO_AUDIO_EXTENSION``
  true if the FDO audio extension is available.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_WPEBACKEND_FDO QUIET wpebackend-fdo-1.0)
set(WPEBackendFDO_COMPILE_OPTIONS ${PC_WPEBACKEND_FDO_CFLAGS_OTHER})
set(WPEBackendFDO_VERSION ${PC_WPEBACKEND_FDO_VERSION})

find_path(WPEBackendFDO_INCLUDE_DIR
    NAMES wpe/fdo.h
    HINTS ${PC_WPEBACKEND_FDO_INCLUDEDIR} ${PC_WPEBACKEND_FDO_INCLUDE_DIRS}
)

find_library(WPEBackendFDO_LIBRARY
    NAMES ${WPEBackendFDO_NAMES} WPEBackend-fdo-1.0
    HINTS ${PC_WPEBACKEND_FDO_LIBDIR} ${PC_WPEBACKEND_FDO_LIBRARY_DIRS}
)

if (WPEBackendFDO_INCLUDE_DIR AND NOT WPEBackendFDO_VERSION)
    if (EXISTS "${WPEBackendFDO_INCLUDE_DIR}/wpe/wpebackend-fdo-version.h")
        file(READ "${WPEBackendFDO_INCLUDE_DIR}/wpe/wpebackend-fdo-version.h" WPEBackendFDO_VERSION_CONTENT)

        string(REGEX MATCH "#define +WPE_FDO_MAJOR_VERSION +([0-9]+)" _dummy "${WPEBackendFDO_VERSION_CONTENT}")
        set(WPEBackendFDO_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +WPE_FDO_MINOR_VERSION +([0-9]+)" _dummy "${WPEBackendFDO_VERSION_CONTENT}")
        set(WPEBackendFDO_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +WPE_FDO_MICRO_VERSION +([0-9]+)" _dummy "${WPEBackendFDO_VERSION_CONTENT}")
        set(WPEBackendFDO_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(WPEBackendFDO_VERSION "${WPEBackendFDO_VERSION_MAJOR}.${WPEBackendFDO_VERSION_MINOR}.${WPEBackendFDO_VERSION_PATCH}")
    endif ()
endif ()

find_path(WPEBackendFDO_AUDIO_EXTENSION
    NAMES wpe/extensions/audio.h
    HINTS ${WPEBackendFDO_INCLUDE_DIR} ${PC_WPEBACKEND_FDO_INCLUDEDIR} ${PC_WPEBACKEND_FDO_INCLUDE_DIRS}
)
if (WPEBackendFDO_AUDIO_EXTENSION)
    set(WPEBackendFDO_AUDIO_EXTENSION TRUE)
else ()
    set(WPEBackendFDO_AUDIO_EXTENSION FALSE)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WPEBackendFDO
    FOUND_VAR WPEBackendFDO_FOUND
    REQUIRED_VARS WPEBackendFDO_LIBRARY WPEBackendFDO_INCLUDE_DIR
    VERSION_VAR WPEBackendFDO_VERSION
)

if (WPEBackendFDO_LIBRARY AND NOT TARGET WPE::FDO)
    add_library(WPE::FDO UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WPE::FDO PROPERTIES
        IMPORTED_LOCATION "${WPEBackendFDO_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${WPEBackendFDO_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${WPEBackendFDO_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(WPEBackendFDO_INCLUDE_DIR WPEBackendFDO_LIBRARY)

if (WPEBackendFDO_FOUND)
    set(WPEBackendFDO_LIBRARIES ${WPEBackendFDO_LIBRARY})
    set(WPEBackendFDO_INCLUDE_DIRS ${WPEBackendFDO_INCLUDE_DIR})
endif ()
