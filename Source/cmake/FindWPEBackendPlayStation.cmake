# Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
FindWPEBackendPlayStation
--------------

Find WPE Backend for PlayStation headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``WPE::PlayStation``
  The WPE Backend for PlayStation library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``WPEBackendPlayStation_FOUND``
  true if (the requested version of) WPE Backend for PlayStation is available.
``WPEBackendPlayStation_VERSION``
  the version of WPE Backend for PlayStation.
``WPEBackendPlayStation_LIBRARIES``
  the libraries to link against to use WPE Backend for PlayStation.
``WPEBackendPlayStation_INCLUDE_DIRS``
  where to find the WPE Backend for PlayStation headers.

#]=======================================================================]

find_path(WPEBackendPlayStation_INCLUDE_DIR
    NAMES wpe/playstation.h
)

find_library(WPEBackendPlayStation_LIBRARY
    NAMES SceWPE
)

if (WPEBackendPlayStation_INCLUDE_DIR AND NOT WPEBackendPlayStation_VERSION)
    if (EXISTS "${WPEBackendPlayStation_INCLUDE_DIR}/wpe/playstation-version.h")
        file(READ "${WPEBackendPlayStation_INCLUDE_DIR}/wpe/playstation-version.h" WPEBackendPlayStation_VERSION_CONTENT)

        string(REGEX MATCH "#define +WPE_PLAYSTATION_MAJOR_VERSION +\\(([0-9]+)\\)" _dummy "${WPEBackendPlayStation_VERSION_CONTENT}")
        set(WPEBackendPlayStation_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +WPE_PLAYSTATION_MINOR_VERSION +\\(([0-9]+)\\)" _dummy "${WPEBackendPlayStation_VERSION_CONTENT}")
        set(WPEBackendPlayStation_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +WPE_PLAYSTATION_MICRO_VERSION +\\(([0-9]+)\\)" _dummy "${WPEBackendPlayStation_VERSION_CONTENT}")
        set(WPEBackendPlayStation_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(WPEBackendPlayStation_VERSION "${WPEBackendPlayStation_VERSION_MAJOR}.${WPEBackendPlayStation_VERSION_MINOR}.${WPEBackendPlayStation_VERSION_PATCH}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WPEBackendPlayStation
    FOUND_VAR WPEBackendPlayStation_FOUND
    REQUIRED_VARS WPEBackendPlayStation_LIBRARY WPEBackendPlayStation_INCLUDE_DIR
    VERSION_VAR WPEBackendPlayStation_VERSION
)

if (WPEBackendPlayStation_LIBRARY AND NOT TARGET WPE::PlayStation)
    add_library(WPE::PlayStation UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WPE::PlayStation PROPERTIES
        IMPORTED_LOCATION "${WPEBackendPlayStation_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${WPEBackendPlayStation_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(WPEBackendPlayStation_INCLUDE_DIR WPEBackendPlayStation_LIBRARY)

if (WPEBackendPlayStation_FOUND)
    set(WPEBackendPlayStation_LIBRARIES ${WPEBackendPlayStation_LIBRARY})
    set(WPEBackendPlayStation_INCLUDE_DIRS ${WPEBackendPlayStation_INCLUDE_DIR})
endif ()
