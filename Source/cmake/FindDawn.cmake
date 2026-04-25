# Copyright (c) 2020 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Intel Corporation nor the names of its contributors may
#   be used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTNativeLAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Try to find Harfbuzz include and library directories.
#
# After successful discovery, this will set for inclusion where needed:
# Dawn_INCLUDE_DIRS - containg the Dawn headers
# Dawn_LIBRARIES - containg the Dawn library

#[=======================================================================[.rst:
FindDawn
--------------

Find Dawn headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Dawn::dawn``
  The Dawn library, if found.

``Dawn::native``
  The Dawn Native library, if found.

``Dawn::wire```
  The Dawn Wire library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Dawn_FOUND``
  true if (the requested version of) Dawn is available.
``Dawn_LIBRARIES``
  the libraries to link against to use Dawn.
``Dawn_INCLUDE_DIRS``
  where to find the Dawn headers.
``Dawn_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_path(WebGPU_INCLUDE_DIR
    NAMES webgpu/webgpu.h
)

find_path(Dawn_INCLUDE_DIR
    NAMES dawn/dawn_proc.h
)

find_library(Dawn_LIBRARY
    NAMES ${Dawn_NAMES} dawn_proc
)

# Find components
if (WebGPU_INCLUDE_DIR AND Dawn_INCLUDE_DIR AND Dawn_LIBRARY)
    set(_Dawn_REQUIRED_LIBS_FOUND ON)
    set(Dawn_LIBS_FOUND "Dawn (required): ${Dawn_LIBRARY}")
else ()
    set(_Dawn_REQUIRED_LIBS_FOUND OFF)
    set(Dawn_LIBS_NOT_FOUND "Dawn (required)")
endif ()

if ("native" IN_LIST Dawn_FIND_COMPONENTS)
    find_path(Dawn_Native_INCLUDE_DIR
        NAMES dawn_native/dawn_native_export.h
    )

    find_library(Dawn_Native_LIBRARY
        NAMES dawn_native
    )

    if (Dawn_Native_INCLUDE_DIR AND Dawn_Native_LIBRARY)
        if (Dawn_FIND_REQUIRED_native)
            list(APPEND Dawn_LIBS_FOUND "Native (required): ${Dawn_Native_LIBRARY}")
        else ()
           list(APPEND Dawn_LIBS_FOUND "Native (optional): ${Dawn_Native_LIBRARY}")
        endif ()
    else ()
        if (Dawn_FIND_REQUIRED_native)
           set(_Dawn_REQUIRED_LIBS_FOUND OFF)
           list(APPEND Dawn_LIBS_NOT_FOUND "Native (required)")
        else ()
           list(APPEND Dawn_LIBS_NOT_FOUND "Native (optional)")
        endif ()
    endif ()
endif ()

if ("wire" IN_LIST Dawn_FIND_COMPONENTS)
    find_path(Dawn_Wire_INCLUDE_DIR
        NAMES dawn_wire/dawn_wire_export.h
    )

    find_library(Dawn_Wire_LIBRARY
        NAMES dawn_wire
    )

    if (Dawn_Wire_INCLUDE_DIR AND Dawn_Wire_LIBRARY)
        if (Dawn_FIND_REQUIRED_wire)
            list(APPEND Dawn_LIBS_FOUND "Wire (required): ${Dawn_Wire_LIBRARY}")
        else ()
           list(APPEND Dawn_LIBS_FOUND "Wire (optional): ${Dawn_Wire_LIBRARY}")
        endif ()
    else ()
        if (Dawn_FIND_REQUIRED_wire)
           set(_Dawn_REQUIRED_LIBS_FOUND OFF)
           list(APPEND Dawn_LIBS_NOT_FOUND "Wire (required)")
        else ()
           list(APPEND Dawn_LIBS_NOT_FOUND "Wire (optional)")
        endif ()
    endif ()
endif ()

if (NOT Dawn_FIND_QUIETLY)
    if (Dawn_LIBS_FOUND)
        message(STATUS "Found the following Dawn libraries:")
        foreach (found ${Dawn_LIBS_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
    if (Dawn_LIBS_NOT_FOUND)
        message(STATUS "The following Dawn libraries were not found:")
        foreach (found ${Dawn_LIBS_NOT_FOUND})
            message(STATUS " ${found}")
        endforeach ()
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Dawn
    FOUND_VAR Dawn_FOUND
    REQUIRED_VARS
        WebGPU_INCLUDE_DIR
        Dawn_INCLUDE_DIR
        Dawn_LIBRARY
        _Dawn_REQUIRED_LIBS_FOUND
)

if (Dawn_LIBRARY AND NOT TARGET Dawn::dawn)
    add_library(Dawn::dawn UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Dawn::dawn PROPERTIES
        IMPORTED_LOCATION "${Dawn_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${WebGPU_INCLUDE_DIR};${Dawn_INCLUDE_DIR}"
    )
endif ()

if (Dawn_Native_LIBRARY AND NOT TARGET Dawn::native)
    add_library(Dawn::native UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Dawn::native PROPERTIES
        IMPORTED_LOCATION "${Dawn_Native_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Dawn_INCLUDE_DIR};${Dawn_Native_INCLUDE_DIR}"
    )
endif ()

if (Dawn_Native_LIBRARY AND NOT TARGET Dawn::wire)
    add_library(Dawn::wire UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Dawn::wire PROPERTIES
        IMPORTED_LOCATION "${Dawn_Wire_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Dawn_INCLUDE_DIR};${Dawn_Wire_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    Dawn_INCLUDE_DIR
    Dawn_LIBRARY
    Dawn_Native_INCLUDE_DIR
    Dawn_Native_LIBRARY
    Dawn_Wire_INCLUDE_DIR
    Dawn_Wire_LIBRARY
)

if (Dawn_FOUND)
    set(Dawn_LIBRARIES
        ${Dawn_LIBRARY}
        ${Dawn_Native_LIBRARY}
        ${Dawn_Wire_LIBRARY}
    )
    set(Dawn_INCLUDE_DIRS
        ${WebGPU_INCLUDE_DIR}
        ${Dawn_INCLUDE_DIR}
        ${Dawn_Native_INCLUDE_DIR}
        ${Dawn_Wire_INCLUDE_DIR}
    )
endif ()
