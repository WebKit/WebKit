#.rst
# FindThunder
# -------------
#
# Finds the Thunder library.
#
# This will define the following variables:
#
# ``THUNDER_FOUND``
#     True if the requested version of Thunder was found
# ``THUNDER_VERSION``
#     The version of Thunder that was found
# ``THUNDER_INCLUDE_DIRS``
#     The Thunder include directories
# ``THUNDER_LIBRARIES``
#     The linker libraries needed to use the Thunder library
#
# Copyright (C) 2020 Metrological
# Copyright (C) 2020 Igalia S.L
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_library(THUNDER_LIBRARY
  NAMES libocdm.so
)
find_path(THUNDER_INCLUDE_DIR
  NAMES open_cdm.h
  PATH_SUFFIXES "WPEFramework/ocdm/"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Thunder
    FOUND_VAR THUNDER_FOUND
    REQUIRED_VARS THUNDER_LIBRARY THUNDER_INCLUDE_DIR
)
if (THUNDER_FOUND)
    set(THUNDER_LIBRARIES ${THUNDER_LIBRARY})
    set(THUNDER_INCLUDE_DIRS ${THUNDER_INCLUDE_DIR})
endif ()

mark_as_advanced(THUNDER_LIBRARY THUNDER_INCLUDE_DIR)

include(FeatureSummary)
set_package_properties(Thunder PROPERTIES
    DESCRIPTION "Thunder DRM framework"
    URL "https://github.com/rdkcentral/Thunder"
)
