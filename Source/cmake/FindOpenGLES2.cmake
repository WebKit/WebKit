# Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
FindOpenGLES2
--------------

Find OpenGL ES headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``OpenGL::GLES``
  The OpenGL ES library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``OpenGLES2_FOUND``
  true if (the requested version of) OpenGL ES is available.
``OpenGLES2_VERSION``
  the version of the OpenGL ES library.
``OpenGLES2_API_VERSION``
  the version of the OpenGL ES API, this may be different than OpenGLES2_VERSION.
``OpenGLES2_LIBRARIES``
  the libraries to link against to use OpenGL ES.
``OpenGLES2_INCLUDE_DIRS``
  where to find the OpenGL ES2 headers.
``OpenGLES2_COMPILE_OPTIONS`
  this should be passed to target_compile_options(), if the
  target is not used for linking.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_OPENGLES2 QUIET glesv2)
set(OpenGLES2_COMPILE_OPTIONS ${PC_OPENGLES2_CFLAGS_OTHER})
set(OpenGLES2_VERSION ${PC_OPENGLES2_VERSION})

find_path(OpenGLES2_INCLUDE_DIR
    NAMES GLES2/gl2.h
    HINTS ${PC_OPENGLES2_INCLUDEDIR} ${PC_OPENGLES2_INCLUDE_DIRS}
)

find_library(OpenGLES2_LIBRARY
    NAMES ${OpenGLES2_NAMES} glesv2 GLESv2
    HINTS ${PC_OPENGLES2_LIBDIR} ${PC_OPENGLES2_LIBRARY_DIRS}
)

# Look for OpenGL ES 3 headers existing to determine the API version
if (OpenGLES2_INCLUDE_DIR)
    if (EXISTS ${OpenGLES2_INCLUDE_DIR}/GLES3/gl32.h)
        set(OpenGLES2_API_VERSION "3.2")
    elseif (EXISTS ${OpenGLES2_INCLUDE_DIR}/GLES3/gl31.h)
        set(OpenGLES2_API_VERSION "3.1")
    elseif (EXISTS ${OpenGLES2_INCLUDE_DIR}/GLES3/gl3.h)
        set(OpenGLES2_API_VERSION "3.0")
    else ()
        set(OpenGLES2_API_VERSION "2.0")
    endif ()

    # Set the version of the library to the API version if not present
    if (NOT OpenGLES2_VERSION)
        set(OpenGLES2_VERSION ${OpenGLES2_API_VERSION})
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenGLES2
    FOUND_VAR OpenGLES2_FOUND
    REQUIRED_VARS OpenGLES2_LIBRARY OpenGLES2_INCLUDE_DIR
    VERSION_VAR OpenGLES2_API_VERSION
)

if (OpenGLES2_LIBRARY AND NOT TARGET OpenGL::GLES)
    add_library(OpenGL::GLES UNKNOWN IMPORTED GLOBAL)
    set_target_properties(OpenGL::GLES PROPERTIES
        IMPORTED_LOCATION "${OpenGLES2_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${OpenGLES2_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${OpenGLES2_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(OpenGLES2_INCLUDE_DIR OpenGLES2_LIBRARY)

if (OpenGLES2_FOUND)
    set(OpenGLES2_LIBRARIES ${OpenGLES2_LIBRARY})
    set(OpenGLES2_INCLUDE_DIRS ${OpenGLES2_INCLUDE_DIR})
endif ()
