# Copyright (C) 2025 Igalia S.L.
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
FindExecinfo
----------

Find if backtrace and backtrace_symbols functions are avaliable

Imported Targets
^^^^^^^^^^^^^^^^

``Execinfo::Execinfo``
  The execinfo library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Execinfo``
  true if backtrace and backtrace_symbols are available

``Execinfo_LIBRARY``
  true if execinfo library is available

#]=======================================================================]

check_include_file("execinfo.h" HAVE_EXECINFO_H)
check_function_exists(backtrace HAVE_BACKTRACE)
check_function_exists(backtrace_symbols HAVE_BACKTRACE_SYMBOLS)

if (NOT HAVE_BACKTRACE)
    find_library(Execinfo_LIBRARY execinfo)
    if (Execinfo_LIBRARY)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_LIBRARIES ${Execinfo_LIBRARY})
        check_function_exists(backtrace HAVE_BACKTRACE)
        check_function_exists(backtrace_symbols HAVE_BACKTRACE_SYMBOLS)
        cmake_pop_check_state()
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Execinfo
    REQUIRED_VARS HAVE_EXECINFO_H HAVE_BACKTRACE HAVE_BACKTRACE_SYMBOLS
)

if (Execinfo_LIBRARY AND NOT TARGET Execinfo::Execinfo)
    add_library(Execinfo::Execinfo UNKNOWN IMPORTED)
    set_target_properties(Execinfo::Execinfo PROPERTIES
        IMPORTED_LOCATION "${Execinfo_LIBRARY}"
    )
endif ()