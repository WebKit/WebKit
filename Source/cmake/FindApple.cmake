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
FindApple
--------------

Find Apple frameworks.

Imported Targets
^^^^^^^^^^^^^^^^

  Apple::<C>

Where ``<C>`` is the name of an Apple framework, for example
``Apple::CoreFoundation``.

Result Variables
^^^^^^^^^^^^^^^^

Apple component libraries are reported in::

  <C>_FOUND - ON if framework was found
  <C>_LIBRARIES - libraries for component

#]=======================================================================]

set(_Apple_REQUIRED_LIBS_FOUND YES)

function(_FIND_APPLE_FRAMEWORK framework)
    set(OPTIONS "")
    set(oneValueArgs HEADER)
    set(multiValueArgs LIBRARY_NAMES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Find include directory and library
    find_path(${framework}_INCLUDE_DIR NAMES ${opt_HEADER})
    find_library(${framework}_LIBRARY NAMES ${opt_LIBRARY_NAMES})

    if (${framework}_INCLUDE_DIR AND ${framework}_LIBRARY)
        add_library(Apple::${framework} UNKNOWN IMPORTED)
        set_target_properties(Apple::${framework} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${${framework}_INCLUDE_DIR}"
            IMPORTED_LOCATION "${${framework}_LIBRARY}"
        )
        set(${framework}_FOUND ON PARENT_SCOPE)
        if (Apple_FIND_REQUIRED_${framework})
            set(Apple_LIBS_FOUND ${Apple_LIBS_FOUND} "${framework} (required): ${${framework}_LIBRARY}" PARENT_SCOPE)
        else ()
            set(Apple_LIBS_FOUND ${Apple_LIBS_FOUND} "${framework} (optional): ${${framework}_LIBRARY}}" PARENT_SCOPE)
        endif ()
    else ()
        if (Apple_FIND_REQUIRED_${framework})
            set(_Apple_REQUIRED_LIBS_FOUND NO PARENT_SCOPE)
            set(Apple_LIBS_NOT_FOUND ${Apple_LIBS_NOT_FOUND} "${framework} (required)" PARENT_SCOPE)
        else ()
            set(Apple_LIBS_NOT_FOUND ${Apple_LIBS_NOT_FOUND} "${framework} (optional)" PARENT_SCOPE)
        endif ()
    endif ()

    mark_as_advanced(${framework}_INCLUDE_DIR ${framework}_LIBRARY)
endfunction()

if ("ApplicationServices" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(ApplicationServices
        HEADER ApplicationServices/ApplicationServices.h
        LIBRARY_NAMES ASL${DEBUG_SUFFIX}
    )
endif ()

if ("AVFoundationCF" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(AVFoundationCF
        HEADER AVFoundationCF/AVFoundationCF.h
        LIBRARY_NAMES AVFoundationCF${DEBUG_SUFFIX}
    )
endif ()

if ("CFNetwork" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(CFNetwork
        HEADER CFNetwork/CFNetwork.h
        LIBRARY_NAMES CFNetwork${DEBUG_SUFFIX}
    )
endif ()

if ("CoreAudio" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(CoreAudio
        HEADER CoreAudio/CoreAudioTypes.h
        LIBRARY_NAMES CoreAudioToolbox${DEBUG_SUFFIX}
    )
endif ()

if ("CoreFoundation" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(CoreFoundation
        HEADER CoreFoundation/CoreFoundation.h
        LIBRARY_NAMES CoreFoundation${DEBUG_SUFFIX} CFlite
    )
endif ()

if ("CoreGraphics" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(CoreGraphics
        HEADER CoreGraphics/CoreGraphics.h
        LIBRARY_NAMES CoreGraphics${DEBUG_SUFFIX}
    )
endif ()

if ("CoreMedia" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(CoreMedia
        HEADER CoreMedia/CoreMedia.h
        LIBRARY_NAMES CoreMedia${DEBUG_SUFFIX}
    )
endif ()

if ("CoreText" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(CoreText
        HEADER CoreText/CoreText.h
        LIBRARY_NAMES CoreText${DEBUG_SUFFIX}
    )
endif ()

if ("CoreVideo" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(CoreVideo
        HEADER CoreVideo/CVBase.h
        LIBRARY_NAMES CoreVideo${DEBUG_SUFFIX}
    )
endif ()

if ("MediaAccessibility" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(MediaAccessibility
        HEADER MediaAccessibility/MediaAccessibility.h
        LIBRARY_NAMES MediaAccessibility${DEBUG_SUFFIX}
    )
endif ()

if ("MediaToolbox" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(MediaToolbox
        HEADER MediaToolbox/MTAudioProcessingTap.h
        LIBRARY_NAMES MediaToolbox${DEBUG_SUFFIX}
    )
endif ()

if ("QuartzCore" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(QuartzCore
        HEADER QuartzCore/QuartzCore.h
        LIBRARY_NAMES QuartzCore${DEBUG_SUFFIX}
    )
endif ()

if ("SarfariTheme" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(SarfariTheme
        HEADER dispatch/dispatch.h
        LIBRARY_NAMES SarfariTheme${DEBUG_SUFFIX}
    )
endif ()

if ("WebKitQuartzCoreAdditions" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(WebKitQuartzCoreAdditions
        HEADER WebKitQuartzCoreAdditions/WebKitQuartzCoreAdditionsBase.h
        LIBRARY_NAMES WebKitQuartzCoreAdditions${DEBUG_SUFFIX}
    )
endif ()

if ("libdispatch" IN_LIST Apple_FIND_COMPONENTS)
    _FIND_APPLE_FRAMEWORK(libdispatch
        HEADER dispatch/dispatch.h
        LIBRARY_NAMES libdispatch${DEBUG_SUFFIX}
    )
endif ()

if (NOT Apple_FIND_QUIETLY)
    if (Apple_LIBS_FOUND)
        message(STATUS "Found the following Apple libraries:")
        foreach (found ${Apple_LIBS_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
    if (Apple_LIBS_NOT_FOUND)
        message(STATUS "The following Apple libraries were not found:")
        foreach (found ${Apple_LIBS_NOT_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Apple
    REQUIRED_VARS _Apple_REQUIRED_LIBS_FOUND
    FAIL_MESSAGE "Failed to find all Apple components"
)
