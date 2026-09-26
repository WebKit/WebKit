# - Try to find LLDB's C++ SB API.
# Once done, this will define
#
#  LLDB_FOUND - LLDB's SB API was found
#  LLDB_INCLUDE_DIRS - the LLDB include directories
#  LLDB_LIBRARIES - link these to use LLDB.
#  LLDB::LLDB - an imported target
#
# Copyright (C) 2026 Igalia S.L.
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

if (APPLE)
    # Xcode's LLDB.framework ships no SB headers, so only Homebrew's is usable.
    set(LLDB_PREFIXES "/opt/homebrew/opt/llvm" "/usr/local/opt/llvm")
else ()
    file(GLOB LLDB_PREFIXES "/usr/lib/llvm-*")
    # newest first (update when llvm 100 is released)
    list(SORT LLDB_PREFIXES COMPARE NATURAL ORDER DESCENDING)
endif ()

find_path(LLDB_INCLUDE_DIR
    NAMES lldb/API/LLDB.h
    PATHS ${LLDB_PREFIXES}
    PATH_SUFFIXES include
)

if (LLDB_INCLUDE_DIR)
    # The library must come from the headers' own install, or a machine with
    # several LLVMs could link one against another's headers.
    get_filename_component(LLDB_PREFIX "${LLDB_INCLUDE_DIR}" DIRECTORY)
    find_library(LLDB_LIBRARY
        NAMES lldb
        HINTS "${LLDB_PREFIX}"
        PATH_SUFFIXES lib lib64
        NO_DEFAULT_PATH
    )
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLDB
    REQUIRED_VARS LLDB_INCLUDE_DIR LLDB_LIBRARY
)

mark_as_advanced(
    LLDB_INCLUDE_DIR
    LLDB_LIBRARY
)

if (LLDB_FOUND)
    set(LLDB_LIBRARIES ${LLDB_LIBRARY})
    set(LLDB_INCLUDE_DIRS ${LLDB_INCLUDE_DIR})

    if (NOT TARGET LLDB::LLDB)
        # CMake gives an imported target's include directories to its users as
        # -isystem, which keeps warnings in LLDB's headers out of WebKit's -Werror.
        add_library(LLDB::LLDB INTERFACE IMPORTED GLOBAL)
        set_target_properties(LLDB::LLDB PROPERTIES
            INTERFACE_LINK_LIBRARIES "${LLDB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LLDB_INCLUDE_DIR}"
        )
    endif ()
endif ()
