# Copyright (C) 2017, 2025 Igalia S.L.
# Copyright (C) 2017 Metrological Group B.V.
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
FindTasn1
---------

Find the libtasn1 headers and library.


Imported Targets
^^^^^^^^^^^^^^^^

``Tasn1::Tasn1``
  The libtasn1 library, if found.

Result Variables
^^^^^^^^^^^^^^^^

``Tasn1_FOUND``
  True if (the requested version of) libtasn1 is available.
``Tasn1_VERSION``
  Version of the libtasn1 library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_Tasn1 QUIET IMPORTED_TARGET libtasn1)

set(Tasn1_COMPILE_OPTIONS ${PC_Tasn1_CFLAGS_OTHER})
set(Tasn1_VERSION ${PC_Tasn1_VERSION})

if (PC_Tasn1_FOUND AND TARGET PkgConfig::PC_Tasn1 AND NOT TARGET Tasn1::Tasn1)
    get_target_property(Tasn1_LIBRARY PkgConfig::PC_Tasn1 INTERFACE_LINK_LIBRARIES)
    list(GET Tasn1_LIBRARY 0 Tasn1_LIBRARY)
    add_library(Tasn1::Tasn1 INTERFACE IMPORTED GLOBAL)
    set_property(TARGET Tasn1::Tasn1 PROPERTY INTERFACE_LINK_LIBRARIES PkgConfig::PC_Tasn1)
endif ()

# Search the library by hand, as a fallback.
if (NOT TARGET Tasn1::Tasn1)
    find_path(Tasn1_INCLUDE_DIR
        NAMES libtasn1.h
        HINTS ${PC_Tasn1_INCLUDEDIR}
              ${PC_Tasn1_INCLUDE_DIRS}
    )
    find_library(Tasn1_LIBRARY
        NAMES tasn1
        HINTS ${PC_Tasn1_LIBDIR}
              ${PC_Tasn1_LIBRARY_DIRS}
    )
    if (Tasn1_LIBRARY)
        add_library(Tasn1::Tasn1 UNKNOWN IMPORTED GLOBAL)
        set_target_properties(Tasn1::Tasn1 PROPERTIES
            IMPORTED_LOCATION "${Tasn1_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${Tasn1_COMPILE_OPTIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${Tasn1_INCLUDE_DIR}"
        )
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tasn1
    REQUIRED_VARS Tasn1_LIBRARY
    VERSION_VAR Tasn1_VERSION
)

mark_as_advanced(
    Tasn1_INCLUDE_DIR
    Tasn1_LIBRARY
)
