# - Try to find LibRice
#
# Copyright (C) 2025 Igalia S.L
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
FindRice
--------------

Find Rice headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``Rice::Proto``
  The Rice proto library, if found.

``Rice::Io``
  The Rice io library, if found.

#]=======================================================================]

find_package(PkgConfig QUIET)

if ("Proto" IN_LIST Rice_FIND_COMPONENTS)
    pkg_check_modules(PC_RICE_PROTO rice-proto)

    if (NOT Rice_VERSION)
        set(Rice_VERSION ${PC_RICE_PROTO_VERSION})
    endif ()

    find_path(RiceProto_INCLUDE_DIR
        NAMES rice-proto.h
        HINTS ${PC_RICE_PROTO_INCLUDEDIR}
              ${PC_RICE_PROTO_INCLUDE_DIRS}
    )

    find_library(Rice_PROTO_LIBRARY
        NAMES rice-proto
        HINTS ${PC_RICE_PROTO_LIBDIR}
              ${PC_RICE_PROTO_LIBRARY_DIRS}
    )

    if (Rice_PROTO_LIBRARY)
        set(Rice_Proto_FOUND TRUE)
    endif ()
endif ()

if ("Io" IN_LIST Rice_FIND_COMPONENTS)
    pkg_check_modules(PC_RICE_IO rice-io)

    if (NOT Rice_VERSION)
        set(Rice_VERSION ${PC_RICE_IO_VERSION})
    endif ()

    find_path(RiceIo_INCLUDE_DIR
        NAMES rice-io.h
        HINTS ${PC_RICE_IO_INCLUDEDIR}
              ${PC_RICE_IO_INCLUDE_DIRS}
    )

    find_library(Rice_IO_LIBRARY
        NAMES rice-io
        HINTS ${PC_RICE_IO_LIBDIR}
              ${PC_RICE_IO_LIBRARY_DIRS}
    )

    if (Rice_IO_LIBRARY)
        set(Rice_Io_FOUND TRUE)
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Rice
    VERSION_VAR Rice_VERSION
    HANDLE_COMPONENTS
)

if (Rice_PROTO_LIBRARY AND NOT TARGET Rice::Proto)
    add_library(Rice::Proto UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Rice::Proto PROPERTIES
        IMPORTED_LOCATION "${Rice_PROTO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${RiceProto_INCLUDE_DIR}"
    )
endif ()

if (Rice_IO_LIBRARY AND NOT TARGET Rice::Io)
    add_library(Rice::Io UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Rice::Io PROPERTIES
        IMPORTED_LOCATION "${Rice_IO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${RiceIo_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
    Rice_PROTO_LIBRARY
    Rice_IO_LIBRARY
)
