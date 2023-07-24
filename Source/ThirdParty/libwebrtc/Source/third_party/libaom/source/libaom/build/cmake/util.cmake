#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_UTIL_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_UTIL_CMAKE_
set(AOM_BUILD_CMAKE_UTIL_CMAKE_ 1)

# Directory where generated sources will be written.
set(AOM_GEN_SRC_DIR "${AOM_CONFIG_DIR}/gen_src")

# Creates a no-op source file in $AOM_GEN_SRC_DIR named $basename.$extension and
# returns the full path to the source file via appending it to the list variable
# referred to by $out_file_list_var parameter.
macro(create_no_op_source_file basename extension out_file_list_var)
  set(no_op_source_file "${AOM_GEN_SRC_DIR}/${basename}_no_op.${extension}")
  file(WRITE "${no_op_source_file}"
       "// Generated file. DO NOT EDIT!\n"
       "// ${target_name} needs a ${extension} file to force link language, \n"
       "// or to silence a harmless CMake warning: Ignore me.\n"
       "void aom_${target_name}_no_op_function(void);\n"
       "void aom_${target_name}_no_op_function(void) {}\n")
  list(APPEND "${out_file_list_var}" "${no_op_source_file}")
endmacro()

# Convenience function for adding a no-op source file to $target_name using
# $extension as the file extension. Wraps create_no_op_source_file().
function(add_no_op_source_file_to_target target_name extension)
  create_no_op_source_file("${target_name}" "${extension}"
                           "no_op_source_file_list")
  target_sources(${target_name} PRIVATE ${no_op_source_file_list})
endfunction()

# Sets the value of the variable referenced by $feature to $value, and reports
# the change to the user via call to message(WARNING ...). $cause is expected to
# be a configuration variable that conflicts with $feature in some way. This
# function is a no-op if $feature is already set to $value.
function(change_config_and_warn feature value cause)
  if(${feature} EQUAL ${value})
    return()
  endif()
  set(${feature} ${value} PARENT_SCOPE)
  if(${value} EQUAL 1)
    set(verb "Enabled")
    set(reason "required for")
  else()
    set(verb "Disabled")
    set(reason "incompatible with")
  endif()
  set(warning_message "${verb} ${feature}, ${reason} ${cause}.")
  message(WARNING "--- ${warning_message}")
endfunction()

# Extracts the version string from $version_file and returns it to the user via
# $version_string_out_var. To achieve this VERSION_STRING_NOSP is located in
# $version_file and then everything but the string literal assigned to the
# variable is removed. Quotes and the leading 'v' are stripped from the returned
# string.
function(extract_version_string version_file version_string_out_var)
  file(STRINGS "${version_file}" aom_version REGEX "VERSION_STRING_NOSP")
  string(REPLACE "#define VERSION_STRING_NOSP " "" aom_version "${aom_version}")
  string(REPLACE "\"" "" aom_version "${aom_version}")
  string(REPLACE " " "" aom_version "${aom_version}")
  string(FIND "${aom_version}" "v" v_pos)
  if(${v_pos} EQUAL 0)
    string(SUBSTRING "${aom_version}" 1 -1 aom_version)
  endif()
  set("${version_string_out_var}" "${aom_version}" PARENT_SCOPE)
endfunction()

# Sets CMake compiler launcher to $launcher_name when $launcher_name is found in
# $PATH. Warns user about ignoring build flag $launcher_flag when $launcher_name
# is not found in $PATH.
function(set_compiler_launcher launcher_flag launcher_name)
  find_program(launcher_path "${launcher_name}")
  if(launcher_path)
    set(CMAKE_C_COMPILER_LAUNCHER "${launcher_path}" PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${launcher_path}" PARENT_SCOPE)
    message("--- Using ${launcher_name} as compiler launcher.")
  else()
    message(
      WARNING "--- Cannot find ${launcher_name}, ${launcher_flag} ignored.")
  endif()
endfunction()

# Sentinel value used to detect when a variable has been set via the -D argument
# passed to CMake on the command line.
set(cmake_cmdline_helpstring "No help, variable specified on the command line.")

# Wrapper macro for set() that does some book keeping to help with storage of
# build configuration information.
#
# Sets the default value for variable $name when the value of $name has not
# already been set via the CMake command line.
#
# The names of variables defaulted through this macro are added to
# $AOM_DETECT_VARS to facilitate build logging and diagnostics.
macro(set_aom_detect_var name value helpstring)
  unset(list_index)
  list(FIND AOM_DETECT_VARS ${name} list_index)
  if(${list_index} EQUAL -1)
    list(APPEND AOM_DETECT_VARS ${name})
  endif()

  # Update the variable only when it does not carry the CMake assigned help
  # string for variables specified via the command line.
  unset(cache_helpstring)
  get_property(cache_helpstring CACHE ${name} PROPERTY HELPSTRING)
  if(NOT "${cache_helpstring}" STREQUAL "${cmake_cmdline_helpstring}")
    set(${name} ${value} CACHE STRING "${helpstring}")
    mark_as_advanced(${name})
  else()
    message(
      WARNING
        "${name} has been set by CMake, but it may be overridden by the build "
        "system during environment detection")
  endif()
endmacro()

# Wrapper macro for set() that does some book keeping to help with storage of
# build configuration information.
#
# Sets the default value for variable $name when the value of $name has not
# already been set via the CMake command line.
#
# The names of variables defaulted through this macro are added to
# $AOM_CONFIG_VARS to facilitate build logging and diagnostics.
macro(set_aom_config_var name value helpstring)
  unset(list_index)
  list(FIND AOM_CONFIG_VARS ${name} list_index)
  if(${list_index} EQUAL -1)
    list(APPEND AOM_CONFIG_VARS ${name})
  endif()

  # Update the variable only when it does not carry the CMake assigned help
  # string for variables specified via the command line.
  unset(cache_helpstring)
  get_property(cache_helpstring CACHE ${name} PROPERTY HELPSTRING)
  if(NOT "${cache_helpstring}" STREQUAL "${cmake_cmdline_helpstring}")
    set(${name} ${value} CACHE STRING "${helpstring}")
  endif()
endmacro()

# Wrapper macro for option() that does some book keeping to help with storage of
# build configuration information.
#
# Sets the default value for variable $name when the value of $name has not
# already been set via the CMake command line.
#
# The names of variables defaulted through this macro are added to
# $AOM_OPTION_VARS to facilitate build logging and diagnostics.
macro(set_aom_option_var name helpstring value)
  unset(list_index)
  list(FIND AOM_OPTION_VARS ${name} list_index)
  if(${list_index} EQUAL -1)
    list(APPEND AOM_OPTION_VARS ${name})
  endif()

  # Update the variable only when it does not carry the CMake assigned help
  # string for variables specified via the command line.
  unset(cache_helpstring)
  get_property(cache_helpstring CACHE ${name} PROPERTY HELPSTRING)
  if(NOT "${cache_helpstring}" STREQUAL "${cmake_cmdline_helpstring}")
    option(${name} "${helpstring}" ${value})
  endif()
endmacro()
