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
FindAndroid
-----------

Find Android NDK headers and libraries.

This module supports checking for the following components:

``Android``
  The base ``libandroid`` library. If no components are specified, this
  will the only one to search for.
``Log``
  The logging ``liblog`` library.

Imported Targets
^^^^^^^^^^^^^^^^

For each supported component, the corresponding `Android::<name>` target
will be defined, for example `Android::Android`, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project, for each
component given its `<name>`:

``Android_<name>_FOUND``
  True if the component `<name>` is available.

#]=======================================================================]

if (NOT ANDROID)
    return()
endif ()

include(CMakePushCheckState)
include(CheckFunctionExists)
include(CheckIncludeFile)

function(_AndroidHandleComponent target)
    if (TARGET Android::${target})
        message(DEBUG "Android::${target} already checked, skipping")
        return()
    endif ()

    if (NOT Android_COMPONENT_${target}_LIBRARY)
        message(CHECK_FAIL "Invalid component name")
        return()
    endif ()

    set(libname "${Android_COMPONENT_${target}_LIBRARY}")
    set(header "${Android_COMPONENT_${target}_HEADER}")
    set(symbol "${Android_COMPONENT_${target}_SYMBOL}")
    set(deps "${Android_COMPONENT_${target}_DEPS}")
    set(deplibs "${libname}")

    foreach (dep IN LISTS deps)
        if (Android_FIND_REQUIRED_${target} AND NOT Android_FIND_REQUIRED_${dep})
            set(Android_FIND_REQUIRED_${dep} TRUE PARENT_SCOPE)
        endif ()
        _AndroidHandleComponent(${dep})
        list(APPEND deplibs Android::${dep})
    endforeach ()

    check_include_file("${header}" Android_COMPONENT_${target}_HAS_HEADER)
    if (NOT Android_COMPONENT_${target}_HAS_HEADER)
        return()
    endif ()

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES "${libname}")
    check_function_exists("${symbol}" Android_COMPONENT_${target}_HAS_SYMBOL)
    cmake_pop_check_state()
    if (NOT Android_COMPONENT_${target}_HAS_SYMBOL)
        return()
    endif ()

    add_library(Android::${target} INTERFACE IMPORTED)
    set_target_properties(Android::${target} PROPERTIES
        INTERFACE_LINK_LIBRARIES "${deplibs}")
    set(Android_${target}_FOUND TRUE PARENT_SCOPE)
endfunction()

function(_AndroidDefineComponent target libname header symbol)
    if (Android_COMPONENT_${target}_LIBRARY)
        message(FATAL_ERROR "Android::${target} component already defined")
    endif ()
    cmake_parse_arguments(PARSE_ARGV 4 opt "" "" "")
    set(Android_COMPONENT_${target}_LIBRARY "${libname}" PARENT_SCOPE)
    set(Android_COMPONENT_${target}_HEADER "${header}" PARENT_SCOPE)
    set(Android_COMPONENT_${target}_SYMBOL "${symbol}" PARENT_SCOPE)
    set(Android_COMPONENT_${target}_DEPS "${opt_UNPARSED_ARGUMENTS}" PARENT_SCOPE)
endfunction()

_AndroidDefineComponent(Android
    android
    android/hardware_buffer.h
    AHardwareBuffer_allocate
)
_AndroidDefineComponent(Log
    log
    android/log.h
    __android_log_print
)

if (NOT Android_FIND_COMPONENTS)
    set(Android_FIND_COMPONENTS Android)
    set(Android_FIND_REQUIRED_Android ${Android_FIND_REQUIRED})
endif ()

foreach (component IN LISTS Android_FIND_COMPONENTS)
    _AndroidHandleComponent(${component})
endforeach ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Android HANDLE_COMPONENTS)
