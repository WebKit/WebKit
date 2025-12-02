# Copyright (C) 2014, 2025 Igalia S.L.
# Copyright (C) 2012 Raphael Kubo da Costa <rakuco@webkit.org>
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
FindSecret
----------

Find the libsecret headers and library.

Imported Targets
^^^^^^^^^^^^^^^^

``Secret::Secret``
  The libsecret library, if found.

Result Variables
^^^^^^^^^^^^^^^^

``Secret_FOUND``
  true if (the requested version of) libsecret is available.
``Secret_VERSION``
  Version of the found libsecret library.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(Secret QUIET IMPORTED_TARGET libsecret-1)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Secret
    REQUIRED_VARS Secret_VERSION Secret_FOUND
    VERSION_VAR Secret_VERSION
)

if (Secret_FOUND AND NOT TARGET Secret::Secret)
    add_library(Secret::Secret INTERFACE IMPORTED GLOBAL)
    set_property(TARGET Secret::Secret PROPERTY
        INTERFACE_LINK_LIBRARIES PkgConfig::Secret
    )
endif ()
