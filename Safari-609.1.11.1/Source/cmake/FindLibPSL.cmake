# - Try to find LibPSL
# This module defines the following variables:
#
#  LIBPSL_FOUND - LibPSL was found
#  LIBPSL_INCLUDE_DIRS - the LibPSL include directories
#  LIBPSL_LIBRARIES - link these to use LibPSL
#
# Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

find_path(LIBPSL_INCLUDE_DIRS
    NAMES libpsl.h
    PATH_SUFFIXES libpsl
)

find_library(LIBPSL_LIBRARIES
    NAMES psl
)

if (LIBPSL_INCLUDE_DIRS)
    if (EXISTS "${LIBPSL_INCLUDE_DIRS}/libpsl.h")
        file(READ "${LIBPSL_INCLUDE_DIRS}/libpsl.h" LIBPSL_VERSION_CONTENT)

        string(REGEX MATCH "#define +PSL_VERSION_MAJOR +([0-9]+)" _dummy "${LIBPSL_VERSION_CONTENT}")
        set(LIBPSL_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +PSL_VERSION_MINOR +([0-9]+)" _dummy "${LIBPSL_VERSION_CONTENT}")
        set(LIBPSL_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +PSL_VERSION_PATCH +([0-9]+)" _dummy "${LIBPSL_VERSION_CONTENT}")
        set(LIBPSL_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(LIBPSL_VERSION "${LIBPSL_VERSION_MAJOR}.${LIBPSL_VERSION_MINOR}.${LIBPSL_VERSION_PATCH}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibPSL REQUIRED_VARS LIBPSL_INCLUDE_DIRS LIBPSL_LIBRARIES
                                         VERSION_VAR LIBPSL_VERSION)

mark_as_advanced(
    LIBPSL_INCLUDE_DIRS
    LIBPSL_LIBRARIES
)
