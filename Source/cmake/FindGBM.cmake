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

#[=======================================================================[.rst:
FindGBM
-------

Find libgbm headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``GBM::GBM``
  The libgbm library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``GBM_FOUND``
  true if (the requested version of) libgbm is available.
``GBM_VERSION``
  the libgbm version.
``GBM_LIBRARIES``
  the library to link against to use libgbm.
``GBM_INCLUDE_DIRS``
  where to find the libgbm headers.
``GBM_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the imported
  target is not used for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_GBM QUIET gbm)
set(GBM_COMPILE_OPTIONS ${PC_GBM_CFLAGS_OTHER})
set(GBM_VERSION ${PC_GBM_VERSION})

find_path(GBM_INCLUDE_DIR
    NAMES gbm.h
    HINTS ${PC_GBM_INCLUDEDIR}
          ${PC_GBM_INCLUDE_DIRS}
)

find_library(GBM_LIBRARY
    NAMES gbm
    HINTS ${PC_GBM_LIBDIR}
          ${PC_GBM_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GBM
    REQUIRED_VARS GBM_LIBRARY GBM_INCLUDE_DIR
    VERSION_VAR GBM_VERSION
)

if (GBM_LIBRARY AND NOT TARGET GBM::GBM)
    add_library(GBM::GBM UNKNOWN IMPORTED GLOBAL)
    set_target_properties(GBM::GBM PROPERTIES
        IMPORTED_LOCATION "${GBM_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${GBM_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${GBM_INCLUDE_DIR}"
    )
    find_package(LibDRM REQUIRED)
    target_link_libraries(GBM::GBM INTERFACE LibDRM::LibDRM)
endif ()

mark_as_advanced(
  GBM_INCLUDE_DIR
  GBM_LIBRARY
)

if (GBM_FOUND)
    set(GBM_LIBRARIES ${GBM_LIBRARY})
    set(GBM_INCLUDE_DIRS ${GBM_INCLUDE_DIR})
endif ()
