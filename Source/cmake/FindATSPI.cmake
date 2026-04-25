# Copyright (c) 2013, 2026 Igalia S.L.
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
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

#[=======================================================================[.rst:
FindATSPI
---------

Find the AT-SPI headers and library.

Imported Targets
^^^^^^^^^^^^^^^^

``ATSPI::ATSPI``
The AT-SPI library, if found.

Result Variables
^^^^^^^^^^^^^^^^

``ATSPI_FOUND``
  true if (the requested version of) the AT-SPI library is available.
``ATSPI_VERSION``
  the version of the AT-SPI library.

#]=======================================================================]

if (DEFINED ATSPI_FIND_VERSION)
    set(ATSPI_PKG_CONFIG_SPEC "atspi-2>=${ATSPI_FIND_VERSION}")
else ()
    set(ATSPI_PKG_CONFIG_SPEC "atspi-2")
endif ()

find_package(PkgConfig QUIET)
pkg_check_modules(ATSPI QUIET IMPORTED_TARGET "${ATSPI_PKG_CONFIG_SPEC}")

if (TARGET PkgConfig::ATSPI AND NOT TARGET ATSPI::ATSPI)
    add_library(ATSPI::ATSPI INTERFACE IMPORTED GLOBAL)
    set_property(TARGET ATSPI::ATSPI PROPERTY
        INTERFACE_LINK_LIBRARIES PkgConfig::ATSPI
    )
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ATSPI
    REQUIRED_VARS ATSPI_VERSION
)
