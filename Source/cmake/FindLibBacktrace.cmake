# - Try to find libbacktrace.
# Once done, this will define
#
#  LIBBACKTRACE_FOUND - libbacktrace was found
#  LIBBACKTRACE_INCLUDE_DIRS - the libbacktrace include directories
#  LIBBACKTRACE_LIBRARIES - link these to use libbacktrace.
#
# Copyright (C) 2023 Igalia S.L.
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

find_path(LIBBACKTRACE_INCLUDE_DIR
    NAMES backtrace.h
)

find_library(LIBBACKTRACE_LIBRARY
    NAMES backtrace
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibBacktrace
  REQUIRED_VARS LIBBACKTRACE_INCLUDE_DIR LIBBACKTRACE_LIBRARY
  VERSION_VAR LIBBACKTRACE_VERSION
)

if (LIBBACKTRACE_LIBRARY AND NOT TARGET LIBBACKTRACE::LIBBACKTRACE)
    add_library(LIBBACKTRACE::LIBBACKTRACE UNKNOWN IMPORTED GLOBAL)
    set_target_properties(LIBBACKTRACE::LIBBACKTRACE PROPERTIES
        IMPORTED_LOCATION "${LIBBACKTRACE_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${LIBBACKTRACE_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBBACKTRACE_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(
  LIBBACKTRACE_INCLUDE_DIR
  LIBBACKTRACE_LIBRARY
)

if (LIBBACKTRACE_FOUND)
    set(LIBBACKTRACE_LIBRARIES ${LIBBACKTRACE_LIBRARY})
    set(LIBBACKTRACE_INCLUDE_DIRS ${LIBBACKTRACE_INCLUDE_DIR})
endif ()
