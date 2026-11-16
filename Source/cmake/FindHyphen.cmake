# - Try to find libhyphen
# Once done, this will define
#
#  HYPHEN_FOUND - system has libhyphen installed.
#  HYPHEN_INCLUDE_DIR - directories which contain the libhyphen headers.
#  HYPHEN_LIBRARY - libraries required to link against libhyphen.
#
# Copyright (C) 2012 Intel Corporation. All rights reserved.
# Copyright (C) 2015, 2025 Igalia S.L.
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
FindHyphen
----------

Find libhyphen header and library.

Imported Targets
^^^^^^^^^^^^^^^^

``Hyphen::Hyphen``
  The libhyphen library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Hyphen_FOUND``
  true if libhyphen is available.

#]=======================================================================]

find_path(Hyphen_INCLUDE_DIR NAMES hyphen.h)
find_library(Hyphen_LIBRARY NAMES hyphen hnj)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hyphen
    REQUIRED_VARS Hyphen_LIBRARY Hyphen_INCLUDE_DIR
)

if (Hyphen_FOUND AND NOT TARGET Hyphen::Hyphen)
    add_library(Hyphen::Hyphen UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Hyphen::Hyphen PROPERTIES
        IMPORTED_LOCATION "${Hyphen_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Hyphen_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    Hyphen_INCLUDE_DIR
    Hyphen_LIBRARY
)

if (Hyphen_FOUND)
    set(Hyphen_LIBRARIES ${Hyphen_LIBRARY})
    set(Hyphen_INCLUDE_DIRS ${Hyphen_INCLUDE_DIR})
endif ()
