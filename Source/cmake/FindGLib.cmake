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
FindGLib
--------

Find the GLib headers and libraries.

Optional Components
^^^^^^^^^^^^^^^^^^^

The ``COMPONENTS`` (or ``OPTIONAL_COMPONENTS``) keyword can be passed to
``find_package()``, the followin GLib components can be searched for:

- ``Module``
- ``Object``
- ``Thread``
- ``Gio``
- ``GioUnix``

For each one of the requested components, a matching target with the
``GLib::`` prefix will be defined.

Imported Targets
^^^^^^^^^^^^^^^^

``GLib::GLib``
  The GLib library; if found.
``GLib::Module``
  The GLib dynamic module loader library; if found. This is an optional
  component.
``GLib::Object``
  The GLib object, parameter, and signal library; if found. This is an
  optional component.
``GLib::Thread``
  The GLib threading support library; if found. This is an optional
  component.
``GLib::Gio``
  The GLib input/output library; if found. This is an optional component.
``GLib::GioUnix``
  Unix additions to the GLib input/output library; if found. This is an
  optional component.

Note that targets are aware of their dependencies. For example ``GLib::Gio``
requires ``GLib::Object``, and targets linking against ``GLib::Gio`` do _not_
need to also specify ``GLib::Object`` manually.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``GLib_FOUND``
  true if (the requested version of) GLib is available.
``GLib_VERSION``
  Version of GLib and its components.
``GLib_Module_FOUND``
  true if the optional ``Module`` component is available.
``GLib_Object_FOUND``
  true if the optional ``Object`` component is available.
``GLib_Thread_FOUND``
  true if the optional ``Thread`` component is available.
``GLib_Gio_FOUND``
  true if the optional ``Gio`` component is available.
``GLib_GioUnix_FOUND``
  true if the optional ``GioUnix`` component is available.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_GLib QUIET glib-2.0)
set(GLib_COMPILE_OPTIONS ${PC_GLib_CFLAGS_OTHER})
set(GLib_VERSION ${PC_GLib_VERSION})

find_library(GLib_LIBRARY
    NAMES glib-2.0
    HINTS ${PC_GLib_LIBDIR}
          ${PC_GLib_LIBRARY_DIRS}
)

find_path(GLib_INCLUDE_DIR
    NAMES glib.h
    HINTS ${PC_GLib_INCLUDEDIR}
          ${PC_GLib_INCLUDE_DIRS}
    PATH_SUFFIXES glib-2.0
)

# The glibconfig.h file is usually installed under $PREFIX/lib/glib-2.0
# instead of $PREFIX/include/glib-2.0, but the needed include path will
# be provided by pkg-config, so we can use the same set of HINTS here,
# plus a path derived from the location of the library.
cmake_path(REPLACE_FILENAME GLib_LIBRARY
    OUTPUT_VARIABLE GLib_LIBRARY_DIR)
cmake_path(APPEND GLib_LIBRARY_DIR
    glib-2.0 include
    OUTPUT_VARIABLE GLib_Config_LIBDIR_INCLUDE_HINT)

find_path(GLib_Config_INCLUDE_DIR
    NAMES glibconfig.h
    HINTS ${PC_GLib_INCLUDEDIR}
          ${PC_GLib_INCLUDE_DIRS}
          ${GLib_Config_LIBDIR_INCLUDE_HINT}
)

set(GLib_INCLUDE_DIRS "${GLib_INCLUDE_DIR};${GLib_Config_INCLUDE_DIR}")

if (NOT GLib_VERSION AND EXISTS "${GLib_Config_INCLUDE_DIR}/glibconfig.h")
    file(READ "${GLib_Config_INCLUDE_DIR}/glibconfig.h" GLib_Config_CONTENT)

    string(REGEX MATCH "#define +GLIB_MAJOR_VERSION +([0-9]+)" _dummy "${GLib_Config_CONTENT")
    set(GLib_VERSION_MAJOR "${CMAKE_MATCH_1}")

    string(REGEX MATCH "#define +GLIB_MINOR_VERSION +([0-9]+)" _dummy "${GLib_Config_CONTENT")
    set(GLib_VERSION_MINOR "${CMAKE_MATCH_1}")

    string(REGEX MATCH "#define +GLIB_MICRO_VERSION +([0-9]+)" _dummy "${GLib_Config_CONTENT")
    set(GLib_VERSION_MICRO "${CMAKE_MATCH_1}")

    set(GLib_VERSION "${GLib_VERSION_MAJOR}.${GLib_VERSION_MINOR}.${GLib_VERSION_MICRO}")
endif ()

if (GLib_LIBRARY AND NOT TARGET GLib::GLib)
    add_library(GLib::GLib UNKNOWN IMPORTED GLOBAL)
    set_target_properties(GLib::GLib PROPERTIES
        IMPORTED_LOCATION "${GLib_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${GLib_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLib_INCLUDE_DIRS}"
    )
endif ()

macro(GLib_DeclareComponent name module library)
    set(GLib_${name}__module "${module}")
    set(GLib_${name}__library "${library}")
    set(GLib_${name}__dependencies "${ARGN}")
endmacro()

function(GLib_HandleComponent name)
    if (TARGET GLib::${name})
        set(GLib_${name}_FOUND TRUE PARENT_SCOPE)
        return()
    endif ()

    set(module "${GLib_${name}__module}")
    set(library "${GLib_${name}__library}")
    set(dependencies "${GLib_${name}__dependencies}")

    if (NOT dependencies)
        set(dependencies GLib)
    endif ()

    foreach (depname IN LISTS dependencies)
        GLib_HandleComponent(${depname})
        set(GLib_${depname}_FOUND "${GLib_${depname}_FOUND}" PARENT_SCOPE)
    endforeach ()

    pkg_check_modules(PC_GLib_${name} QUIET ${module})

    find_library(GLib_${name}_LIBRARY
        NAMES ${library}
        HINTS ${PC_GLib_LIBDIR}
              ${PC_GLib_LIBRARY_DIRS}
              ${PC_GLib_${name}_LIBDIR}
              ${PC_GLib_${name}_LIBRARY_DIRS}
    )
    if (NOT GLib_${name}_LIBRARY)
        return()
    endif ()

    set(GLib_${name}_FOUND TRUE PARENT_SCOPE)
    list(TRANSFORM dependencies PREPEND GLib::)
    add_library(GLib::${name} UNKNOWN IMPORTED GLOBAL)
    set_target_properties(GLib::${name} PROPERTIES IMPORTED_LOCATION "${GLib_${name}_LIBRARY}")
    target_include_directories(GLib::${name} INTERFACE ${PC_GLib_${name}_INCLUDEDIR})
    target_include_directories(GLib::${name} INTERFACE ${PC_GLib_${name}_INCLUDE_DIRS})
    target_compile_options(GLib::${name} INTERFACE ${PC_GLib_${name}_CFLAGS})
    target_compile_options(GLib::${name} INTERFACE ${PC_GLib_${name}_CFLAGS_OTHER})
    target_link_libraries(GLib::${name} INTERFACE ${dependencies})
endfunction()

GLib_DeclareComponent(Module  gmodule-2.0  gmodule-2.0)
GLib_DeclareComponent(Object  gobject-2.0  gobject-2.0)
GLib_DeclareComponent(Thread  gthread-2.0  gthread-2.0)
GLib_DeclareComponent(Gio     gio-2.0      gio-2.0 Object)
GLib_DeclareComponent(GioUnix gio-unix-2.0 gio-2.0 Gio)

foreach (component IN LISTS GLib_FIND_COMPONENTS)
    GLib_HandleComponent(${component})
endforeach ()

mark_as_advanced(
    GLib_INCLUDE_DIR
    GLib_Config_INCLUDE_DIR
    GLib_LIBRARY
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLib
    REQUIRED_VARS GLib_LIBRARY GLib_INCLUDE_DIRS
    VERSION_VAR GLib_VERSION
    HANDLE_COMPONENTS
)
