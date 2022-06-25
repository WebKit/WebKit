# - Try to find GTK+ 3.x or 4.x
#
# Copyright (C) 2012 Raphael Kubo da Costa <rakuco@webkit.org>
# Copyright (C) 2013, 2015, 2020 Igalia S.L.
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
FindGTK
-------

Find GTK headers and libraries.

Optional Components
^^^^^^^^^^^^^^^^^^^

The ``COMPONENTS`` (or ``OPTIONAL_COMPONENTS``) keyword can be passed to
``find_package()``, the following GTK components can be searched for:

- ``unix-print``


Imported Targets
^^^^^^^^^^^^^^^^

``GTK::GTK``
  The GTK library, if found.
``GTK::UnixPrint``
  The GTK unix-print library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``GTK_FOUND``
  true if (the requested version of) GTK is available.
``GTK_UNIX_PRINT_FOUND``
  true if the ``unix-print`` component is available.
``GTK_4``
  whether GTK 4 was detected
``GTK_3``
  whether GTK 3 was detected
``GTK_VERSION``
  the version of GTK.
``GTK_SUPPORTS_BROADWAY``
  true if the Broadway target is built into GTK.
``GTK_SUPPORTS_QUARTZ``
  true if the Quartz target is built into GTK.
``GTK_SUPPORTS_WAYLAND``
  true if the Wayland target is built into GTK.
``GTK_SUPPORTS_WIN32``
  true if the Windows target is built into GTK.
``GTK_SUPPORTS_X11``
  true if the X11 target is built into GTK.

#]=======================================================================]

if (NOT DEFINED GTK_FIND_VERSION)
    message(FATAL_ERROR "No GTK version specified")
endif ()

if (GTK_FIND_VERSION VERSION_LESS 3.90)
    set(GTK_PC_MODULE "gtk+-3.0")
    set(GTK_PC_UNIX_PRINT_MODULE "gtk+-unix-print-3.0")
    set(GTK_4 FALSE)
    set(GTK_3 TRUE)
else ()
    set(GTK_PC_MODULE "gtk4")
    set(GTK_PC_UNIX_PRINT_MODULE "gtk4-unix-print")
    set(GTK_4 TRUE)
    set(GTK_3 FALSE)
endif ()

find_package(PkgConfig QUIET)
pkg_check_modules(GTK IMPORTED_TARGET ${GTK_PC_MODULE})

set(GTK_VERSION_OK TRUE)
if (GTK_VERSION)
    if (GTK_FIND_VERSION_EXACT)
        if (NOT("${GTK_FIND_VERSION}" VERSION_EQUAL "${GTK_VERSION}"))
            set(GTK_VERSION_OK FALSE)
        endif ()
    else ()
        if ("${GTK_VERSION}" VERSION_LESS "${GTK_FIND_VERSION}")
            set(GTK_VERSION_OK FALSE)
        endif ()
    endif ()
endif ()

# Set all the GTK_SUPPORTS_<target> variables to FALSE initially.
foreach (gtk_target broadway quartz wayland win32 x11)
    string(TOUPPER "GTK_SUPPORTS_${gtk_target}" gtk_target)
    set(${gtk_target} FALSE)
endforeach ()

if (GTK_VERSION AND GTK_VERSION_OK)
    # Fetch the "targets" variable and set GTK_SUPPORTS_<target>.
    pkg_get_variable(GTK_TARGETS ${GTK_PC_MODULE} targets)
    separate_arguments(GTK_TARGETS)
    foreach (gtk_target ${GTK_TARGETS})
        string(TOUPPER "GTK_SUPPORTS_${gtk_target}" gtk_target)
        set(${gtk_target} TRUE)
    endforeach ()
endif ()

if (TARGET PkgConfig::GTK AND NOT TARGET GTK::GTK)
    add_library(GTK::GTK INTERFACE IMPORTED GLOBAL)
    set_property(TARGET GTK::GTK PROPERTY
        INTERFACE_LINK_LIBRARIES PkgConfig::GTK
    )
endif ()

# Try to find additional components
foreach (gtk_component ${GTK_FIND_COMPONENTS})
    if (NOT "${gtk_component}" STREQUAL unix-print)
        message(FATAL_ERROR "Invalid component name: ${gtk_component}")
    endif ()
    pkg_check_modules(GTK_UNIX_PRINT IMPORTED_TARGET "${GTK_PC_UNIX_PRINT_MODULE}")
    if (GTK_FIND_REQUIRED_unix-print AND NOT GTK_UNIX_PRINT_FOUND)
        message(FATAL_ERROR "Component unix-print not found")
    endif ()
    if (TARGET PkgConfig::GTK_UNIX_PRINT AND NOT TARGET GTK::UnixPrint)
        add_library(GTK::UnixPrint INTERFACE IMPORTED GLOBAL)
        set_property(TARGET GTK::UnixPrint PROPERTY
            INTERFACE_LINK_LIBRARIES PkgConfig::GTK_UNIX_PRINT)
    endif ()
endforeach ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GTK DEFAULT_MSG GTK_VERSION GTK_VERSION_OK)
