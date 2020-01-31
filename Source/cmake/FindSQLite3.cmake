# Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
FindSQLite3
--------------

Find SQLite3 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``SQLite::SQLite3``
  The SQLite3 library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``SQLite3_FOUND``
  true if (the requested version of) SQLite3 is available.
``SQLite3_VERSION``
  the version of SQLite3.
``SQLite3_LIBRARIES``
  the libraries to link against to use SQLite3.
``SQLite3_INCLUDE_DIRS``
  where to find the SQLite3 headers.
``SQLite3_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

# TODO: Remove when cmake_minimum_version bumped to 3.14.

include(FindPkgConfig)
pkg_check_modules(PC_SQLITE3 QUIET sqlite3)
set(SQLite3_COMPILE_OPTIONS ${PC_SQLITE3_CFLAGS_OTHER})
set(SQLite3_VERSION ${PC_SQLITE3_VERSION})

find_path(SQLite3_INCLUDE_DIR
    NAMES sqlite3.h
    HINTS ${PC_SQLITE3_INCLUDEDIR} ${PC_SQLITE3_INCLUDE_DIR}
)

find_library(SQLite3_LIBRARY
    NAMES ${SQLite3_NAMES} sqlite3
    HINTS ${PC_SQLITE3_LIBDIR} ${PC_SQLITE3_LIBRARY_DIRS}
)

if (SQLite3_INCLUDE_DIR AND NOT SQLite3_VERSION)
    if (EXISTS "${SQLite3_INCLUDE_DIR}/sqlite3.h")
        file(STRINGS ${SQLite3_INCLUDE_DIR}/sqlite3.h _ver_line
            REGEX "^#define SQLITE_VERSION  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
            LIMIT_COUNT 1)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
            SQLite3_VERSION "${_ver_line}")
        unset(_ver_line)
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLite3
    FOUND_VAR SQLite3_FOUND
    REQUIRED_VARS SQLite3_LIBRARY SQLite3_INCLUDE_DIR
    VERSION_VAR SQLite3_VERSION
)

if (SQLite3_LIBRARY AND NOT TARGET SQLite::SQLite3)
    add_library(SQLite::SQLite3 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SQLite::SQLite3 PROPERTIES
        IMPORTED_LOCATION "${SQLite3_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${SQLite3_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(SQLite3_INCLUDE_DIR SQLite3_LIBRARIES)

if (SQLite3_FOUND)
    set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
    set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
endif ()
