# - Try to find CairoGL
# Once done, this will define
#
#  CAIRO_GL_FOUND - system has CairoGL
#  CAIRO_GL_INCLUDE_DIRS - the CairoGL include directories
#  CAIRO_GL_LIBRARIES - link these to use CairoGL
#
# Copyright (C) 2014 Igalia S.L.
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
pkg_check_modules(CAIRO_GL cairo-gl)

# At the moment CairoGL does not add any extra cflags and libraries, so we can
# safely ignore CAIRO_GL_LIBRARIES and CAIRO_GL_INCLUDE_DIRS for the moment.
foreach (_component ${CairoGL_FIND_COMPONENTS})
    string(TOUPPER ${_component} _UPPER_NAME)
    string(REGEX REPLACE "-" "_" _UPPER_NAME ${_UPPER_NAME})
    pkg_check_modules(${_UPPER_NAME} ${_component})
    set(CAIRO_GL_INCLUDE_DIRS ${CAIRO_GL_INCLUDE_DIRS} ${_UPPER_NAME}_INCLUDE_DIRS)
    set(CAIRO_GL_LIBRARIES ${CAIRO_GL_LIBRARIES} ${_UPPER_NAME}_LIBRARIES)
endforeach ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CairoGL DEFAULT_MSG CAIRO_GL_INCLUDE_DIRS CAIRO_GL_LIBRARIES)

