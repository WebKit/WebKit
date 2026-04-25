# Copyright (C) 2023 Igalia S.L.
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
FindFlite
---------

Find flite headers and libraries.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Flite_FOUND``
  true if (the requested version of) flite is available.
``Flite_VERSION``
  the version of flite.
``Flite_LIBRARIES``
  the libraries to link against to use flite.
``Flite_INCLUDE_DIRS``
  where to find the flite headers.

#]=======================================================================]

find_path(Flite_INCLUDE_DIR
    NAMES flite.h
    PATH_SUFFIXES flite
)

find_library(Flite_LIBRARY
    NAMES ${Flite_NAMES} flite
)

# Find version
if (Flite_INCLUDE_DIR AND NOT Flite_VERSION)
    if (EXISTS "${Flite_INCLUDE_DIR}/flite_version.h")
        file(READ "${Flite_INCLUDE_DIR}/flite_version.h" Flite_VERSION_CONTENT)

        string(REGEX MATCH "#define +FLITE_PROJECT_VERSION +\"([0-9]+\.[0-9]+)\"" _dummy "${Flite_VERSION_CONTENT}")
        set(Flite_VERSION "${CMAKE_MATCH_1}")
    endif ()
endif ()

# Find components
if (Flite_INCLUDE_DIR AND Flite_LIBRARY)
    set(Flite_LIBS_FOUND "flite (required): ${Flite_LIBRARY}")
else ()
    set(Flite_LIBS_NOT_FOUND "flite (required)")
endif ()

macro(FIND_FLITE_COMPONENT _component_prefix _library)
    find_library(${_component_prefix}_LIBRARY
        NAMES ${_library}
    )

    if (${_component_prefix}_LIBRARY)
        list(APPEND Flite_LIBS_FOUND "${_library} (required): ${${_component_prefix}_LIBRARY}")
    else ()
        list(APPEND Flite_LIBS_NOT_FOUND "${_library} (required)")
    endif ()
endmacro()
FIND_FLITE_COMPONENT(Flite_Usenglish flite_usenglish)
FIND_FLITE_COMPONENT(Flite_Cmu_Grapheme_Lang flite_cmu_grapheme_lang)
FIND_FLITE_COMPONENT(Flite_Cmu_Grapheme_Lex flite_cmu_grapheme_lex)
FIND_FLITE_COMPONENT(Flite_Cmu_Indic_Lang flite_cmu_indic_lang)
FIND_FLITE_COMPONENT(Flite_Cmu_Indic_Lex flite_cmu_indic_lex)
FIND_FLITE_COMPONENT(Flite_Cmulex flite_cmulex)
FIND_FLITE_COMPONENT(Flite_Cmu_Time_Awb flite_cmu_time_awb)
FIND_FLITE_COMPONENT(Flite_Cmu_Us_Awb flite_cmu_us_awb)
FIND_FLITE_COMPONENT(Flite_Cmu_Us_Kal16 flite_cmu_us_kal16)
FIND_FLITE_COMPONENT(Flite_Cmu_Us_Kal flite_cmu_us_kal)
FIND_FLITE_COMPONENT(Flite_Cmu_Us_Rms flite_cmu_us_rms)
FIND_FLITE_COMPONENT(Flite_Cmu_Us_Slt flite_cmu_us_slt)

if (NOT Flite_FIND_QUIETLY)
    if (Flite_LIBS_FOUND)
        message(STATUS "Found the following Flite libraries:")
        foreach (found ${Flite_LIBS_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
    if (Flite_LIBS_NOT_FOUND)
        message(STATUS "The following Flite libraries were not found:")
        foreach (found ${Flite_LIBS_NOT_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Flite
    FOUND_VAR Flite_FOUND
    REQUIRED_VARS Flite_INCLUDE_DIR Flite_LIBRARY
    VERSION_VAR Flite_VERSION
)

if (Flite_INCLUDE_DIR AND Flite_LIBRARY)
    set(Flite_FOUND 1)
else ()
    set(Flite_FOUND 0)
endif ()

mark_as_advanced(
    Flite_INCLUDE_DIR
    Flite_LIBRARY
    Flite_Usenglish_LIBRARY
    Flite_Cmu_Grapheme_Lang_LIBRARY
    Flite_Cmu_Grapheme_Lex_LIBRARY
    Flite_Cmu_Indic_Lang_LIBRARY
    Flite_Cmu_Indic_Lex_LIBRARY
    Flite_Cmulex_LIBRARY
    Flite_Cmu_Time_Awb_LIBRARY
    Flite_Cmu_Us_Awb_LIBRARY
    Flite_Cmu_Us_Kal16_LIBRARY
    Flite_Cmu_Us_Kal_LIBRARY
    Flite_Cmu_Us_Rms_LIBRARY
    Flite_Cmu_Us_Slt_LIBRARY
    Flite_FOUND
)

if (Flite_FOUND)
    set(Flite_LIBRARIES
        ${Flite_LIBRARY}
        ${Flite_Usenglish_LIBRARY}
        ${Flite_Cmu_Grapheme_Lang_LIBRARY}
        ${Flite_Cmu_Grapheme_Lex_LIBRARY}
        ${Flite_Cmu_Indic_Lang_LIBRARY}
        ${Flite_Cmu_Indic_Lex_LIBRARY}
        ${Flite_Cmulex_LIBRARY}
        ${Flite_Cmu_Time_Awb_LIBRARY}
        ${Flite_Cmu_Us_Awb_LIBRARY}
        ${Flite_Cmu_Us_Kal16_LIBRARY}
        ${Flite_Cmu_Us_Kal_LIBRARY}
        ${Flite_Cmu_Us_Rms_LIBRARY}
        ${Flite_Cmu_Us_Slt_LIBRARY}
    )
    set(Flite_INCLUDE_DIRS ${Flite_INCLUDE_DIR})
endif ()
