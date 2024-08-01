#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_
set(AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_ 1)

include("${AOM_ROOT}/build/cmake/util.cmake")

# Translate $flag to one which MSVC understands, and write the new flag to the
# variable named by $translated_flag (or unset it, when MSVC needs no flag).
function(get_msvc_intrinsic_flag flag translated_flag)
  if("${flag}" STREQUAL "-mavx")
    set(${translated_flag} "/arch:AVX" PARENT_SCOPE)
  elseif("${flag}" STREQUAL "-mavx2")
    set(${translated_flag} "/arch:AVX2" PARENT_SCOPE)
  else()

    # MSVC does not need flags for intrinsics flavors other than AVX/AVX2.
    unset(${translated_flag} PARENT_SCOPE)
  endif()
endfunction()

# Adds an object library target. Terminates generation if $flag is not supported
# by the current compiler. $flag is the intrinsics flag required by the current
# compiler, and is added to the compile flags for all sources in $sources.
# $opt_name is used to name the target. $target_to_update is made dependent upon
# the created target.
#
# Note: this function always updates the aom, and aom_static targets because
# OBJECT libraries have rules that disallow the direct addition of .o files to
# them as dependencies. Static and shared libraries do not have this limitation.
function(add_intrinsics_object_library flag opt_name target_to_update sources)
  if("${${sources}}" STREQUAL "")
    return()
  endif()
  set(target_name ${target_to_update}_${opt_name}_intrinsics)
  add_library(${target_name} OBJECT ${${sources}})
  set_property(TARGET ${target_name} PROPERTY FOLDER ${AOM_TARGET_CPU})

  # MSVC does not need flags for intrinsics flavors other than AVX/AVX2.
  # However, for clang-cl, the default is SSE2, and the MSVC frontend does not
  # provide any flags to enable SSE3 up to SSE4.1. So we need to restrict the
  # usage of MSVC-style flags to only the real MSVC.
  if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    get_msvc_intrinsic_flag("${flag}" "flag")
  endif()

  if("${flag}" STREQUAL "-mavx2")
    unset(FLAG_SUPPORTED)
    check_c_compiler_flag("-mno-avx256-split-unaligned-load" FLAG_SUPPORTED)
    if(${FLAG_SUPPORTED})
      set(flag "${flag} -mno-avx256-split-unaligned-load")
    endif()

    unset(FLAG_SUPPORTED)
    check_c_compiler_flag("-mno-avx256-split-unaligned-store" FLAG_SUPPORTED)
    if(${FLAG_SUPPORTED})
      set(flag "${flag} -mno-avx256-split-unaligned-store")
    endif()
  endif()

  if(flag)
    separate_arguments(flag)
    target_compile_options(${target_name} PUBLIC ${flag})
  endif()

  target_sources(aom PRIVATE $<TARGET_OBJECTS:${target_name}>)
  if(BUILD_SHARED_LIBS)
    target_sources(aom_static PRIVATE $<TARGET_OBJECTS:${target_name}>)
  endif()

  # Add the new lib target to the global list of aom library targets.
  list(APPEND AOM_LIB_TARGETS ${target_name})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction()

# Adds sources in list named by $sources to $target and adds $flag to the
# compile flags for each source file.
function(add_intrinsics_source_to_target flag target sources)
  target_sources(${target} PRIVATE ${${sources}})
  if(MSVC)
    get_msvc_intrinsic_flag("${flag}" "flag")
  endif()
  if(flag)
    foreach(source ${${sources}})
      set_property(SOURCE ${source} APPEND PROPERTY COMPILE_FLAGS ${flag})
    endforeach()
  endif()
endfunction()

# Writes object format for the current target to the var named by $out_format,
# or terminates the build when the object format for the current target is
# unknown.
function(get_asm_obj_format out_format)
  if("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho64")
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS"
           OR "${AOM_TARGET_SYSTEM}" STREQUAL "CYGWIN"
           OR "${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      set(objformat "win64")
    else()
      set(objformat "elf64")
    endif()
  elseif("${AOM_TARGET_CPU}" STREQUAL "x86")
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho32")
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS"
           OR "${AOM_TARGET_SYSTEM}" STREQUAL "CYGWIN"
           OR "${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      set(objformat "win32")
    else()
      set(objformat "elf32")
    endif()
  else()
    message(
      FATAL_ERROR "Unknown obj format: ${AOM_TARGET_CPU}-${AOM_TARGET_SYSTEM}")
  endif()

  set(${out_format} ${objformat} PARENT_SCOPE)
endfunction()

# Adds library target named $lib_name for ASM files in variable named by
# $asm_sources. Builds an output directory path from $lib_name. Links $lib_name
# into the aom library target(s). Generates a C file with an unused no-op
# function to ensure that all cmake generators can determine the linker
# language, and that build tools don't complain that an object exposes no
# symbols.
#
# In Xcode-based builds every step described above happens twice, and
# directory/target/object names are updated to include _shared and _static
# suffixes.
function(add_asm_library lib_name asm_sources)
  if("${${asm_sources}}" STREQUAL "")
    return()
  endif()

  if(XCODE)
    # CMake's generator does not output a build rule for Nasm files. Moreover,
    # it makes Xcode believe Nasm files are of type "sourcecode" instead of
    # "sourcecode.nasm", which prevents even the default rule from applying.
    # This default rule is broken, though, because it doesn't apply any of the
    # flags specified for ASM_NASM. See https://discourse.cmake.org/t/building-
    # nasm-files-with-xcode/7934
    list(APPEND asm_configs "static")
    if(BUILD_SHARED_LIBS)
      list(APPEND asm_configs "shared")
    endif()

    set(as_executable "${CMAKE_ASM_NASM_COMPILER}")
    if(NOT as_executable)
      set(as_executable "${CMAKE_ASM_COMPILER}")
    endif()

    foreach(asm_config ${asm_configs})
      set(asm_lib_name ${lib_name}_${asm_config})
      set(asm_lib_obj_dir "${AOM_CONFIG_DIR}/asm_objects/${asm_lib_name}")
      if(NOT EXISTS "${asm_lib_obj_dir}")
        file(MAKE_DIRECTORY "${asm_lib_obj_dir}")
      endif()

      foreach(asm_source ${${asm_sources}})
        get_filename_component(asm_source_name "${asm_source}" NAME)
        set(asm_object "${asm_lib_obj_dir}/${asm_source_name}.o")
        add_custom_command(OUTPUT "${asm_object}"
                           COMMAND ${as_executable} ARGS ${AOM_AS_FLAGS}
                                   -I${AOM_ROOT}/ -I${AOM_CONFIG_DIR}/ -o
                                   "${asm_object}" "${asm_source}"
                           DEPENDS "${asm_source}"
                           COMMENT "Building ASM object ${asm_object}"
                           WORKING_DIRECTORY "${AOM_CONFIG_DIR}"
                           VERBATIM)
        if(BUILD_SHARED_LIBS AND "${asm_config}" STREQUAL "static")
          target_sources(aom_static PRIVATE "${asm_object}")
        else()
          target_sources(aom PRIVATE "${asm_object}")
        endif()
      endforeach()
    endforeach()
  else()
    # For non-Xcode generators, CMake does not need extra help. The language
    # support takes care of it.
    set(asm_lib_name ${lib_name})

    add_library(${asm_lib_name} OBJECT ${${asm_sources}})
    target_include_directories(${asm_lib_name}
                               PRIVATE ${AOM_ROOT} ${AOM_CONFIG_DIR})
    target_compile_options(${asm_lib_name} PRIVATE ${AOM_AS_FLAGS})
    set_property(TARGET ${asm_lib_name} PROPERTY FOLDER ${AOM_TARGET_CPU})
    if(BUILD_SHARED_LIBS)
      target_sources(aom_static PRIVATE "$<TARGET_OBJECTS:${asm_lib_name}>")
    endif()
    target_sources(aom PRIVATE "$<TARGET_OBJECTS:${asm_lib_name}>")

    # Add the new lib target to the global list of aom library targets.
    list(APPEND AOM_LIB_TARGETS ${asm_lib_name})
  endif()

  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction()

# Terminates generation if nasm found in PATH does not meet requirements.
# Currently checks only for presence of required object formats and support for
# the -Ox argument (multipass optimization).
function(test_nasm)
  execute_process(COMMAND ${CMAKE_ASM_NASM_COMPILER} -hf
                  OUTPUT_VARIABLE nasm_helptext)

  if(NOT "${nasm_helptext}" MATCHES "-Ox")
    message(
      FATAL_ERROR "Unsupported nasm: multipass optimization not supported.")
  endif()

  if("${AOM_TARGET_CPU}" STREQUAL "x86")
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      if(NOT "${nasm_helptext}" MATCHES "macho32")
        message(
          FATAL_ERROR "Unsupported nasm: macho32 object format not supported.")
      endif()
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS"
           OR "${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      if(NOT "${nasm_helptext}" MATCHES "win32")
        message(
          FATAL_ERROR "Unsupported nasm: win32 object format not supported.")
      endif()
    else()
      if(NOT "${nasm_helptext}" MATCHES "elf32")
        message(
          FATAL_ERROR "Unsupported nasm: elf32 object format not supported.")
      endif()
    endif()
  else()
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      if(NOT "${nasm_helptext}" MATCHES "macho64")
        message(
          FATAL_ERROR "Unsupported nasm: macho64 object format not supported.")
      endif()
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS"
           OR "${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      if(NOT "${nasm_helptext}" MATCHES "win64")
        message(
          FATAL_ERROR "Unsupported nasm: win64 object format not supported.")
      endif()
    else()
      if(NOT "${nasm_helptext}" MATCHES "elf64")
        message(
          FATAL_ERROR "Unsupported nasm: elf64 object format not supported.")
      endif()
    endif()
  endif()
endfunction()

# Adds build command for generation of rtcd C source files using
# build/cmake/rtcd.pl. $config is the input perl file, $output is the output C
# include file, $source is the C source file, and $symbol is used for the symbol
# argument passed to rtcd.pl.
function(add_rtcd_build_step config output source symbol)
  add_custom_command(
    OUTPUT ${output}
    COMMAND ${PERL_EXECUTABLE} ARGS "${AOM_ROOT}/build/cmake/rtcd.pl"
            --arch=${AOM_TARGET_CPU}
            --sym=${symbol} ${AOM_RTCD_FLAGS}
            --config=${AOM_CONFIG_DIR}/config/aom_config.h ${config} > ${output}
    DEPENDS "${AOM_ROOT}/build/cmake/rtcd.pl" ${config}
    COMMENT "Generating ${output}"
    WORKING_DIRECTORY ${AOM_CONFIG_DIR}
    VERBATIM)
  set_property(SOURCE ${source} PROPERTY OBJECT_DEPENDS ${output})
  set_property(SOURCE ${output} PROPERTY GENERATED TRUE)
endfunction()
