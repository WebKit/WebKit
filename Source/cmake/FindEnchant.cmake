# Copyright (C) 2025 Igalia S.L.
# Copyright (C) 2012 Samsung Electronics
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
FindEnchant
-----------

Find the enchant-2 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Enchant::Enchant``
  The enchant-2 library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Enchant_FOUND``
  true if enchant-2 is available.
``Enchant_VERSION``
  Version of enchant-2.

#]=======================================================================]


find_package(PkgConfig QUIET)
pkg_check_modules(PC_Enchant QUIET IMPORTED_TARGET enchant-2)

set(Enchant_COMPILE_OPTIONS ${PC_Enchant_CFLAGS_OTHER})
set(Enchant_VERSION ${PC_Enchant_VERSION})

if (PC_Enchant_FOUND AND TARGET PkgConfig::PC_Enchant AND NOT TARGET Enchant::Enchant)
    get_target_property(Enchant_LIBRARY PkgConfig::PC_Enchant INTERFACE_LINK_LIBRARIES)
    list(GET Enchant_LIBRARY 0 Enchant_LIBRARY)
    add_library(Enchant::Enchant INTERFACE IMPORTED GLOBAL)
    set_property(TARGET Enchant::Enchant PROPERTY INTERFACE_LINK_LIBRARIES PkgConfig::PC_Enchant)
endif ()

# Search the library by hand, as a fallback.
if (NOT TARGET Enchant::Enchant)
    find_path(Enchant_INCLUDE_DIR
        NAMES enchant.h
        HINTS ${PC_Enchant_INCLUDEDIR}
              ${PC_Enchant_INCLUDE_DIRS}
    )
    find_library(Enchant_LIBRARY
        NAMES enchant-2
        HINTS ${PC_Enchant_LIBDIR}
              ${PC_Enchant_LIBRARY_DIRS}
    )
    if (Enchant_LIBRARY)
        add_library(Enchant::Enchant UNKNOWN IMPORTED GLOBAL)
        set_target_properties(Enchant::Enchant PROPERTIES
            IMPORTED_LOCATION "${Enchant_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${Enchant_COMPILE_OPTIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${Enchant_INCLUDE_DIR}"
        )
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Enchant
    REQUIRED_VARS Enchant_LIBRARY
    VERSION_VAR Enchant_VERSION
)

mark_as_advanced(
    Enchant_INCLUDE_DIR
    Enchant_LIBRARY
)
