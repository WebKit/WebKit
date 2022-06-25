# - Try to find Pixman
# This module defines the following variables:
#
#  PIXMAN_FOUND - Pixman was found
#  PIXMAN_INCLUDE_DIRS - the Pixman include directories
#  PIXMAN_LIBRARIES - link these to use Pixman
#
# Copyright (C) 2018 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

find_path(PIXMAN_INCLUDE_DIRS
    NAMES pixman.h
    PATH_SUFFIXES pixman-1
)

find_library(PIXMAN_LIBRARIES
    NAMES pixman-1
)

if (PIXMAN_INCLUDE_DIRS)
    if (EXISTS "${PIXMAN_INCLUDE_DIRS}/pixman-version.h")
        file(READ "${PIXMAN_INCLUDE_DIRS}/pixman-version.h" PIXMAN_VERSION_CONTENT)

        string(REGEX MATCH "#define +PIXMAN_VERSION_MAJOR +([0-9]+)" _dummy "${PIXMAN_VERSION_CONTENT}")
        set(PIXMAN_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +PIXMAN_VERSION_MINOR +([0-9]+)" _dummy "${PIXMAN_VERSION_CONTENT}")
        set(PIXMAN_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +PIXMAN_VERSION_MICRO +([0-9]+)" _dummy "${PIXMAN_VERSION_CONTENT}")
        set(PIXMAN_VERSION_MICRO "${CMAKE_MATCH_1}")

        set(PIXMAN_VERSION "${PIXMAN_VERSION_MAJOR}.${PIXMAN_VERSION_MINOR}.${PIXMAN_VERSION_MICRO}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pixman REQUIRED_VARS PIXMAN_INCLUDE_DIRS PIXMAN_LIBRARIES
                                         VERSION_VAR PIXMAN_VERSION)

mark_as_advanced(
    PIXMAN_INCLUDE_DIRS
    PIXMAN_LIBRARIES
)
