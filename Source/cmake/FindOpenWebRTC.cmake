# - Try to find OpenWebRTC.
# Once done, this will define
#
#  OPENWEBRTC_FOUND - system has OpenWebRTC.
#  OPENWEBRTC_INCLUDE_DIRS - the OpenWebRTC include directories
#  OPENWEBRTC_LIBRARIES - link these to use OpenWebRTC.
#
# Copyright (C) 2015 Igalia S.L.
# Copyright (C) 2015 Metrological.
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
pkg_check_modules(PC_OPENWEBRTC openwebrtc-0.3 openwebrtc-gst-0.3)

set(VERSION_OK TRUE)
if (PC_OPENWEBRTC_VERSION)
    if (OPENWEBRTC_FIND_VERSION_EXACT)
        if (NOT("${OPENWEBRTC_FIND_VERSION}" VERSION_EQUAL "${PC_OPENWEBRTC_VERSION}"))
            set(VERSION_OK FALSE)
        endif ()
    else ()
        if ("${OPENWEBRTC_VERSION}" VERSION_LESS "${PC_OPENWEBRTC_FIND_VERSION}")
            set(VERSION_OK FALSE)
        endif ()
    endif ()
endif ()

find_path(OPENWEBRTC_INCLUDE_DIRS
    NAMES owr/owr.h
    HINTS ${PC_OPENWEBRTC_INCLUDEDIR} ${PC_OPENWEBRTC_INCLUDE_DIRS}
)

find_library(OPENWEBRTC_LIBRARY
    NAMES openwebrtc
    HINTS ${PC_OPENWEBRTC_LIBDIR} ${PC_OPENWEBRTC_LIBRARY_DIRS}
)
find_library(OPENWEBRTC_GST_LIBRARY
    NAMES openwebrtc_gst
    HINTS ${PC_OPENWEBRTC_LIBDIR} ${PC_OPENWEBRTC_LIBRARY_DIRS}
)

set(OPENWEBRTC_LIBRARIES
    ${OPENWEBRTC_LIBRARY}
    ${OPENWEBRTC_GST_LIBRARY}
)

mark_as_advanced(OPENWEBRTC_INCLUDE_DIRS OPENWEBRTC_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenWebRTC REQUIRED_VARS OPENWEBRTC_INCLUDE_DIRS OPENWEBRTC_LIBRARIES VERSION_OK
                                  FOUND_VAR OPENWEBRTC_FOUND)
