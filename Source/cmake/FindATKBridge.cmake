# Copyright (C) 2019, 2020 Igalia S.L.
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
FindATKBridge
-------------

Find the ATK-SPI2 bridge headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``ATK::Bridge``
  The ATK SPI2 bridge library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``ATKBridge_FOUND``
  true if (the requested version of) the ATK SPI2 bridge is available.
``ATKBridge_VERSION``
  the version of the ATK SPI2 bridge.
``ATKBridge_LIBRARIES``
  the libraries to link against to use the ATK SPI2 bridge.
``ATKBridge_INCLUDE_DIRS``
  where to find the ATK SPI2 bridge headers.
``ATKBridge_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the target
  is not used for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_ATK_BRIDGE QUIET atk-bridge-2.0)
set(ATKBridge_COMPILE_OPTIONS ${PC_ATK_BRIDGE_CFLAGS_OTHER})
set(ATKBridge_VERSION ${PC_ATK_BRIDGE_VERSION})

find_path(ATKBridge_INCLUDE_DIR
    NAMES atk-bridge.h
    HINTS ${PC_ATK_BRIDGE_INCLUDEDIR} ${PC_ATK_BRIDGE_INCLUDE_DIR}
    PATH_SUFFIXES at-spi2-atk/2.0
)

find_library(ATKBridge_LIBRARY
    NAMES ${ATKBridge_NAMES} atk-bridge-2.0
    HINTS ${PC_ATK_BRIDGE_LIBDIR} ${PC_ATK_BRIDGE_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ATKBridge
    FOUND_VAR ATKBridge_FOUND
    REQUIRED_VARS ATKBridge_LIBRARY ATKBridge_INCLUDE_DIR
    VERSION_VAR ATKBridge_VERSION
)

if (ATKBridge_LIBRARY AND NOT TARGET ATK::Bridge)
    add_library(ATK::Bridge UNKNOWN IMPORTED GLOBAL)
    set_target_properties(ATK::Bridge PROPERTIES
        IMPORTED_LOCATION "${ATKBridge_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${ATKBridge_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${ATKBridge_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(ATKBridge_INCLUDE_DIR ATKBridge_LIBRARY)

if (ATKBridge_FOUND)
    set(ATKBridge_LIBRARIES ${ATKBridge_LIBRARY})
    set(ATKBridge_INCLUDE_DIRS ${ATKBridge_INCLUDE_DIR})
endif ()
