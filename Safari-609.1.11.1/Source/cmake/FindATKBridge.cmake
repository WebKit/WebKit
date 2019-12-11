# - Try to find ATK
# Once done, this will define
#
#  ATK_BRIDGE_INCLUDE_DIRS - the ATK bridge include drectories
#  ATK_BRIDGE_LIBRARIES - link these to use ATK bridge
#
# Copyright (C) 2019 Igalia S.L.
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

find_package(PkgConfig)
pkg_check_modules(PC_ATK_BRIDGE atk-bridge-2.0)

find_path(ATK_BRIDGE_INCLUDE_DIRS
    NAMES atk-bridge.h
    HINTS ${PC_ATK_BRIDGE_INCLUDEDIR}
          ${PC_ATK_BRIDGE_INCLUDE_DIRS}
    PATH_SUFFIXES at-spi2-atk/2.0
)

find_library(ATK_BRIDGE_LIBRARIES
    NAMES atk-bridge-2.0
    HINTS ${PC_ATK_BRIDGE_LIBRARY_DIRS}
          ${PC_ATK_BRIDGE_LIBDIR}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ATK_BRIDGE REQUIRED_VARS ATK_BRIDGE_INCLUDE_DIRS ATK_BRIDGE_LIBRARIES
                                      VERSION_VAR   PC_ATK_BRIDGE_VERSION)
mark_as_advanced(
    ATK_BRIDGE_INCLUDE_DIRS
    ATK_BRIDGE_LIBRARIES
)
