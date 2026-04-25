#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_AOM_CONFIGURE_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_AOM_CONFIGURE_CMAKE_
set(AOM_BUILD_CMAKE_AOM_CONFIGURE_CMAKE_ 1)

include(FindThreads)

include("${AOM_ROOT}/build/cmake/aom_config_defaults.cmake")
include("${AOM_ROOT}/build/cmake/aom_experiment_deps.cmake")
include("${AOM_ROOT}/build/cmake/aom_optimization.cmake")
include("${AOM_ROOT}/build/cmake/compiler_flags.cmake")
include("${AOM_ROOT}/build/cmake/compiler_tests.cmake")
include("${AOM_ROOT}/build/cmake/util.cmake")

if(DEFINED CONFIG_LOWBITDEPTH)
  message(WARNING "CONFIG_LOWBITDEPTH has been removed. \
    Use -DFORCE_HIGHBITDEPTH_DECODING=1 instead of -DCONFIG_LOWBITDEPTH=0 \
    and -DFORCE_HIGHBITDEPTH_DECODING=0 instead of -DCONFIG_LOWBITDEPTH=1.")
  if(NOT CONFIG_LOWBITDEPTH)
    set(FORCE_HIGHBITDEPTH_DECODING
        1
        CACHE STRING "${cmake_cmdline_helpstring}" FORCE)
  endif()
endif()

if(FORCE_HIGHBITDEPTH_DECODING AND NOT CONFIG_AV1_HIGHBITDEPTH)
  change_config_and_warn(CONFIG_AV1_HIGHBITDEPTH 1
                         "FORCE_HIGHBITDEPTH_DECODING")
endif()

if(CONFIG_THREE_PASS AND NOT CONFIG_AV1_DECODER)
  change_config_and_warn(CONFIG_THREE_PASS 0 "CONFIG_AV1_DECODER=0")
endif()

# Generate the user config settings.
list(APPEND aom_build_vars ${AOM_CONFIG_VARS} ${AOM_OPTION_VARS})
foreach(cache_var ${aom_build_vars})
  get_property(cache_var_helpstring CACHE ${cache_var} PROPERTY HELPSTRING)
  if(cache_var_helpstring STREQUAL cmake_cmdline_helpstring)
    set(AOM_CMAKE_CONFIG "${AOM_CMAKE_CONFIG} -D${cache_var}=${${cache_var}}")
  endif()
endforeach()
string(STRIP "${AOM_CMAKE_CONFIG}" AOM_CMAKE_CONFIG)

# Detect target CPU.
if(NOT AOM_TARGET_CPU)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" cpu_lowercase)
  if(cpu_lowercase STREQUAL "amd64" OR cpu_lowercase STREQUAL "x86_64")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(AOM_TARGET_CPU "x86")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(AOM_TARGET_CPU "x86_64")
    else()
      message(
        FATAL_ERROR "--- Unexpected pointer size (${CMAKE_SIZEOF_VOID_P}) for\n"
                    "      CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}\n"
                    "      CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}\n"
                    "      CMAKE_GENERATOR=${CMAKE_GENERATOR}\n")
    endif()
  elseif(cpu_lowercase STREQUAL "i386" OR cpu_lowercase STREQUAL "x86")
    set(AOM_TARGET_CPU "x86")
  elseif(cpu_lowercase MATCHES "^arm")
    set(AOM_TARGET_CPU "${cpu_lowercase}")
  elseif(cpu_lowercase MATCHES "aarch64")
    set(AOM_TARGET_CPU "arm64")
  elseif(cpu_lowercase MATCHES "^ppc")
    set(AOM_TARGET_CPU "ppc")
  elseif(cpu_lowercase MATCHES "^riscv")
    set(AOM_TARGET_CPU "riscv")
  else()
    message(WARNING "The architecture ${CMAKE_SYSTEM_PROCESSOR} is not "
                    "supported, falling back to the generic target")
    set(AOM_TARGET_CPU "generic")
  endif()
endif()

if(CMAKE_TOOLCHAIN_FILE) # Add toolchain file to config string.
  if(IS_ABSOLUTE "${CMAKE_TOOLCHAIN_FILE}")
    file(RELATIVE_PATH toolchain_path "${AOM_CONFIG_DIR}"
         "${CMAKE_TOOLCHAIN_FILE}")
  else()
    set(toolchain_path "${CMAKE_TOOLCHAIN_FILE}")
  endif()
  set(toolchain_string "-DCMAKE_TOOLCHAIN_FILE=\\\"${toolchain_path}\\\"")
  set(AOM_CMAKE_CONFIG "${toolchain_string} ${AOM_CMAKE_CONFIG}")
else()

  # Add detected CPU to the config string.
  set(AOM_CMAKE_CONFIG "-DAOM_TARGET_CPU=${AOM_TARGET_CPU} ${AOM_CMAKE_CONFIG}")
endif()
set(AOM_CMAKE_CONFIG "-G \\\"${CMAKE_GENERATOR}\\\" ${AOM_CMAKE_CONFIG}")
file(RELATIVE_PATH source_path "${AOM_CONFIG_DIR}" "${AOM_ROOT}")
set(AOM_CMAKE_CONFIG "cmake ${source_path} ${AOM_CMAKE_CONFIG}")
string(STRIP "${AOM_CMAKE_CONFIG}" AOM_CMAKE_CONFIG)

message("--- aom_configure: Detected CPU: ${AOM_TARGET_CPU}")
set(AOM_TARGET_SYSTEM ${CMAKE_SYSTEM_NAME})

string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type_lowercase)
if(build_type_lowercase STREQUAL "debug")
  set(CONFIG_DEBUG 1)
endif()

if(BUILD_SHARED_LIBS)
  set(CONFIG_PIC 1)
  set(CONFIG_SHARED 1)
elseif(NOT CONFIG_PIC)
  # Update the variable only when it does not carry the CMake assigned help
  # string for variables specified via the command line. This allows the user to
  # force CONFIG_PIC=0.
  unset(cache_helpstring)
  get_property(cache_helpstring CACHE CONFIG_PIC PROPERTY HELPSTRING)
  if(NOT "${cache_helpstring}" STREQUAL "${cmake_cmdline_helpstring}")
    aom_check_c_compiles("pie_check" "
                          #if !(__pie__ || __PIE__)
                          #error Neither __pie__ or __PIE__ are set
                          #endif
                          extern void unused(void);
                          void unused(void) {}" HAVE_PIE)

    if(HAVE_PIE)
      # If -fpie or -fPIE are used ensure the assembly code has PIC enabled to
      # avoid DT_TEXTRELs: /usr/bin/ld: warning: creating DT_TEXTREL in a PIE
      set(CONFIG_PIC 1)
      message(
        "CONFIG_PIC enabled for position independent executable (PIE) build")
    endif()
  endif()
  unset(cache_helpstring)
endif()

if(NOT MSVC)
  if(CONFIG_PIC)

    # TODO(tomfinegan): clang needs -pie in CMAKE_EXE_LINKER_FLAGS for this to
    # work.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if(AOM_TARGET_SYSTEM STREQUAL "Linux"
       AND AOM_TARGET_CPU MATCHES "^armv[78]")
      set(AOM_AS_FLAGS ${AOM_AS_FLAGS} --defsym PIC=1)
    else()
      set(AOM_AS_FLAGS ${AOM_AS_FLAGS} -DPIC)
    endif()
  endif()
endif()

if(AOM_TARGET_CPU STREQUAL "x86" OR AOM_TARGET_CPU STREQUAL "x86_64")
  find_program(CMAKE_ASM_NASM_COMPILER yasm $ENV{YASM_PATH})
  if(NOT CMAKE_ASM_NASM_COMPILER OR ENABLE_NASM)
    unset(CMAKE_ASM_NASM_COMPILER CACHE)
    find_program(CMAKE_ASM_NASM_COMPILER nasm $ENV{NASM_PATH})
  endif()

  include(CheckLanguage)
  check_language(ASM_NASM)
  if(CMAKE_ASM_NASM_COMPILER)
    get_asm_obj_format("objformat")
    unset(CMAKE_ASM_NASM_OBJECT_FORMAT)
    set(CMAKE_ASM_NASM_OBJECT_FORMAT ${objformat})
    enable_language(ASM_NASM)
    if(CMAKE_ASM_NASM_COMPILER_ID STREQUAL "NASM")
      test_nasm()
    endif()
    # Xcode requires building the objects manually, so pass the object format
    # flag.
    if(XCODE)
      set(AOM_AS_FLAGS -f ${objformat} ${AOM_AS_FLAGS})
    endif()
  else()
    message(
      FATAL_ERROR
        "Unable to find assembler. Install 'yasm' or 'nasm.' "
        "To build without optimizations, add -DAOM_TARGET_CPU=generic to "
        "your cmake command line.")
  endif()
  string(STRIP "${AOM_AS_FLAGS}" AOM_AS_FLAGS)
elseif(AOM_TARGET_CPU MATCHES "arm")
  if(AOM_TARGET_SYSTEM STREQUAL "Darwin")
    if(NOT CMAKE_ASM_COMPILER)
      set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
    endif()
    set(AOM_AS_FLAGS -arch ${AOM_TARGET_CPU} -isysroot ${CMAKE_OSX_SYSROOT})
  elseif(AOM_TARGET_SYSTEM STREQUAL "Windows")
    if(NOT CMAKE_ASM_COMPILER)
      set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER} "-c -mimplicit-it=always")
    endif()
  else()
    if(NOT CMAKE_ASM_COMPILER)
      set(CMAKE_ASM_COMPILER as)
    endif()
  endif()
  include(CheckLanguage)
  check_language(ASM)
  if(NOT CMAKE_ASM_COMPILER)
    message(
      FATAL_ERROR
        "Unable to find assembler and optimizations are enabled."
        "Searched for ${CMAKE_ASM_COMPILER}. Install it, add it to your path,"
        "or set the assembler directly by adding "
        "-DCMAKE_ASM_COMPILER=<assembler path> to your CMake command line."
        "To build without optimizations, add -DAOM_TARGET_CPU=generic to your "
        "cmake command line.")
  endif()
  enable_language(ASM)
  string(STRIP "${AOM_AS_FLAGS}" AOM_AS_FLAGS)
endif()

if(CONFIG_ANALYZER)
  find_package(wxWidgets REQUIRED adv base core)
  include(${wxWidgets_USE_FILE})
endif()

if(NOT MSVC AND CMAKE_C_COMPILER_ID MATCHES "GNU\|Clang")
  set(CONFIG_GCC 1)
endif()

if(CONFIG_GCOV)
  message("--- Testing for CONFIG_GCOV support.")
  require_linker_flag("-fprofile-arcs -ftest-coverage")
  require_compiler_flag("-fprofile-arcs -ftest-coverage" YES)
endif()

if(CONFIG_GPROF)
  message("--- Testing for CONFIG_GPROF support.")
  require_compiler_flag("-pg" YES)
endif()

if(AOM_TARGET_SYSTEM MATCHES "Darwin\|Linux\|Windows\|Android")
  set(CONFIG_OS_SUPPORT 1)
endif()

# Define macros that affect Windows headers.
if(AOM_TARGET_SYSTEM STREQUAL "Windows")
  # The default _WIN32_WINNT value in MinGW is 0x0502 (Windows XP with SP2). Set
  # it to 0x0601 (Windows 7).
  add_compiler_flag_if_supported("-D_WIN32_WINNT=0x0601")
  # Quiet warnings related to fopen, printf, etc.
  add_compiler_flag_if_supported("-D_CRT_SECURE_NO_WARNINGS")
endif()

#
# Fix CONFIG_* dependencies. This must be done before including cpu.cmake to
# ensure RTCD_CONFIG_* are properly set.
fix_experiment_configs()

# Don't just check for pthread.h, but use the result of the full pthreads
# including a linking check in FindThreads above.
set(HAVE_PTHREAD_H ${CMAKE_USE_PTHREADS_INIT})
aom_check_source_compiles("unistd_check" "#include <unistd.h>" HAVE_UNISTD_H)

if(NOT WIN32)
  aom_push_var(CMAKE_REQUIRED_LIBRARIES "m")
  aom_check_c_compiles("fenv_check" "#define _GNU_SOURCE
                        #include <fenv.h>
                        void unused(void) {
                          (void)unused;
                          (void)feenableexcept(FE_DIVBYZERO | FE_INVALID);
                        }" HAVE_FEXCEPT)
  aom_pop_var(CMAKE_REQUIRED_LIBRARIES)
endif()

include("${AOM_ROOT}/build/cmake/cpu.cmake")

if(ENABLE_CCACHE)
  set_compiler_launcher(ENABLE_CCACHE ccache)
endif()

if(ENABLE_DISTCC)
  set_compiler_launcher(ENABLE_DISTCC distcc)
endif()

if(ENABLE_GOMA)
  set_compiler_launcher(ENABLE_GOMA gomacc)
endif()

if(NOT CONFIG_AV1_DECODER AND NOT CONFIG_AV1_ENCODER)
  message(FATAL_ERROR "Decoder and encoder disabled, nothing to build.")
endif()

if(DECODE_HEIGHT_LIMIT OR DECODE_WIDTH_LIMIT)
  change_config_and_warn(CONFIG_SIZE_LIMIT 1
                         "DECODE_HEIGHT_LIMIT and DECODE_WIDTH_LIMIT")
endif()

if(CONFIG_SIZE_LIMIT)
  if(NOT DECODE_HEIGHT_LIMIT OR NOT DECODE_WIDTH_LIMIT)
    message(FATAL_ERROR "When setting CONFIG_SIZE_LIMIT, DECODE_HEIGHT_LIMIT "
                        "and DECODE_WIDTH_LIMIT must be set.")
  endif()
endif()

# Test compiler flags.
if(MSVC)
  # It isn't possible to specify C99 conformance for MSVC.
  add_cxx_flag_if_supported("/std:c++17")
  add_compiler_flag_if_supported("/W3")

  # Disable MSVC warnings that suggest making code non-portable.
  add_compiler_flag_if_supported("/wd4996")
  if(ENABLE_WERROR)
    add_compiler_flag_if_supported("/WX")
  endif()

  # Compile source files in parallel
  add_compiler_flag_if_supported("/MP")
else()
  require_c_flag("-std=c99" YES)
  if(CYGWIN AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # The GNU C++ compiler in Cygwin needs the -std=gnu++* flag to make the
    # POSIX function declarations visible in the Standard C Library headers.
    require_cxx_flag_nomsvc("-std=gnu++17" YES)
  else()
    require_cxx_flag_nomsvc("-std=c++17" YES)
  endif()
  add_compiler_flag_if_supported("-Wall")
  add_compiler_flag_if_supported("-Wdisabled-optimization")
  add_compiler_flag_if_supported("-Wextra")
  # Prior to version 3.19.0 cmake would fail to parse the warning emitted by gcc
  # with this flag. Note the order of this check and -Wextra-semi-stmt is
  # important due to is_flag_present() matching substrings with string(FIND
  # ...).
  if(CMAKE_VERSION VERSION_LESS "3.19"
     AND CMAKE_C_COMPILER_ID STREQUAL "GNU"
     AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
    add_cxx_flag_if_supported("-Wextra-semi")
  else()
    add_compiler_flag_if_supported("-Wextra-semi")
  endif()
  add_compiler_flag_if_supported("-Wextra-semi-stmt")
  add_compiler_flag_if_supported("-Wfloat-conversion")
  add_compiler_flag_if_supported("-Wformat=2")
  add_c_flag_if_supported("-Wimplicit-function-declaration")
  add_compiler_flag_if_supported("-Wlogical-op")
  add_compiler_flag_if_supported("-Wmissing-declarations")
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compiler_flag_if_supported("-Wmissing-prototypes")
  else()
    add_c_flag_if_supported("-Wmissing-prototypes")
  endif()
  add_compiler_flag_if_supported("-Wpointer-arith")
  add_compiler_flag_if_supported("-Wshadow")
  add_compiler_flag_if_supported("-Wshorten-64-to-32")
  add_compiler_flag_if_supported("-Wsign-compare")
  add_compiler_flag_if_supported("-Wstring-conversion")
  add_compiler_flag_if_supported("-Wtype-limits")
  add_compiler_flag_if_supported("-Wundef")
  add_compiler_flag_if_supported("-Wuninitialized")
  add_compiler_flag_if_supported("-Wunreachable-code-aggressive")
  add_compiler_flag_if_supported("-Wunused")
  add_compiler_flag_if_supported("-Wvla")
  add_cxx_flag_if_supported("-Wc++20-extensions")
  add_cxx_flag_if_supported("-Wc++23-extensions")

  if(CMAKE_C_COMPILER_ID MATCHES "GNU" AND SANITIZE MATCHES "address|undefined")

    # This combination has more stack overhead, so we account for it by
    # providing higher stack limit than usual.
    add_c_flag_if_supported("-Wstack-usage=285000")
    add_cxx_flag_if_supported("-Wstack-usage=270000")
  elseif(CONFIG_RD_DEBUG) # Another case where higher stack usage is expected.
    add_c_flag_if_supported("-Wstack-usage=135000")
    add_cxx_flag_if_supported("-Wstack-usage=240000")
  else()
    add_c_flag_if_supported("-Wstack-usage=100000")
    add_cxx_flag_if_supported("-Wstack-usage=240000")
  endif()

  if(CMAKE_C_COMPILER_ID MATCHES "GNU" AND SANITIZE MATCHES "address")
    # Disable no optimization warning when compiling with sanitizers
    add_compiler_flag_if_supported("-Wno-disabled-optimization")
  endif()

  # Quiet gcc 6 vs 7 abi warnings:
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77728
  if(AOM_TARGET_CPU MATCHES "arm")
    add_cxx_flag_if_supported("-Wno-psabi")
  endif()

  if(ENABLE_WERROR)
    add_compiler_flag_if_supported("-Werror")
  endif()

  if(build_type_lowercase MATCHES "rel")
    add_compiler_flag_if_supported("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")
  endif()
  add_compiler_flag_if_supported("-D_LARGEFILE_SOURCE")
  add_compiler_flag_if_supported("-D_FILE_OFFSET_BITS=64")

  # Do not allow implicit vector type conversions on Clang builds (this is
  # already the default on GCC builds).
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # Clang 8.0.1 (in Cygwin) doesn't support -flax-vector-conversions=none.
    add_compiler_flag_if_supported("-flax-vector-conversions=none")
  endif()
endif()

# Prior to r23, or with ANDROID_USE_LEGACY_TOOLCHAIN_FILE set,
# android.toolchain.cmake would set normal (non-cache) versions of variables
# like CMAKE_C_FLAGS_RELEASE which would mask the ones added to the cache
# variable in add_compiler_flag_if_supported(), etc. As a workaround we add
# everything accumulated in AOM_C/CXX_FLAGS to the normal versions. This could
# also be addressed by reworking the flag tests and adding the results directly
# to target_compile_options() as in e.g., libgav1, but that's a larger task.
# https://github.com/android/ndk/wiki/Changelog-r23#changes
if(ANDROID
   AND ("${ANDROID_NDK_MAJOR}" LESS 23 OR ANDROID_USE_LEGACY_TOOLCHAIN_FILE))
  foreach(lang C;CXX)
    string(STRIP "${AOM_${lang}_FLAGS}" AOM_${lang}_FLAGS)
    if(AOM_${lang}_FLAGS)
      foreach(config ${AOM_${lang}_CONFIGS})
        set(${config} "${${config}} ${AOM_${lang}_FLAGS}")
      endforeach()
    endif()
  endforeach()
endif()

set(AOM_LIB_LINK_TYPE PUBLIC)
if(EMSCRIPTEN)

  # Avoid CMake generation time errors resulting from collisions with the form
  # of target_link_libraries() used by Emscripten.cmake.
  unset(AOM_LIB_LINK_TYPE)
endif()

# Generate aom_config templates.
set(aom_config_asm_template "${AOM_CONFIG_DIR}/config/aom_config.asm.cmake")
set(aom_config_h_template "${AOM_CONFIG_DIR}/config/aom_config.h.cmake")
execute_process(
  COMMAND ${CMAKE_COMMAND}
          -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR} -DAOM_ROOT=${AOM_ROOT} -P
          "${AOM_ROOT}/build/cmake/generate_aom_config_templates.cmake")

# Generate aom_config.{asm,h}.
configure_file("${aom_config_asm_template}"
               "${AOM_CONFIG_DIR}/config/aom_config.asm")
configure_file("${aom_config_h_template}"
               "${AOM_CONFIG_DIR}/config/aom_config.h")

# Read the current git hash.
find_package(Git)
if(NOT GIT_FOUND)
  message("--- Git missing, version will be read from CHANGELOG.")
endif()

string(TIMESTAMP year "%Y")
configure_file("${AOM_ROOT}/build/cmake/aom_config.c.template"
               "${AOM_CONFIG_DIR}/config/aom_config.c")

# Find Perl and generate the RTCD sources.
find_package(Perl)
if(NOT PERL_FOUND)
  message(FATAL_ERROR "Perl is required to build libaom.")
endif()

set(AOM_RTCD_CONFIG_FILE_LIST "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
                              "${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
                              "${AOM_ROOT}/av1/common/av1_rtcd_defs.pl")
set(AOM_RTCD_HEADER_FILE_LIST "${AOM_CONFIG_DIR}/config/aom_dsp_rtcd.h"
                              "${AOM_CONFIG_DIR}/config/aom_scale_rtcd.h"
                              "${AOM_CONFIG_DIR}/config/av1_rtcd.h")
set(AOM_RTCD_SOURCE_FILE_LIST "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
                              "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
                              "${AOM_ROOT}/av1/common/av1_rtcd.c")
set(AOM_RTCD_SYMBOL_LIST aom_dsp_rtcd aom_scale_rtcd av1_rtcd)
list(LENGTH AOM_RTCD_SYMBOL_LIST AOM_RTCD_CUSTOM_COMMAND_COUNT)
math(EXPR AOM_RTCD_CUSTOM_COMMAND_COUNT "${AOM_RTCD_CUSTOM_COMMAND_COUNT} - 1")

foreach(NUM RANGE ${AOM_RTCD_CUSTOM_COMMAND_COUNT})
  list(GET AOM_RTCD_CONFIG_FILE_LIST ${NUM} AOM_RTCD_CONFIG_FILE)
  list(GET AOM_RTCD_HEADER_FILE_LIST ${NUM} AOM_RTCD_HEADER_FILE)
  list(GET AOM_RTCD_SOURCE_FILE_LIST ${NUM} AOM_RTCD_SOURCE_FILE)
  list(GET AOM_RTCD_SYMBOL_LIST ${NUM} AOM_RTCD_SYMBOL)
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} "${AOM_ROOT}/build/cmake/rtcd.pl"
      --arch=${AOM_TARGET_CPU}
      --sym=${AOM_RTCD_SYMBOL} ${AOM_RTCD_FLAGS}
      --config=${AOM_CONFIG_DIR}/config/aom_config.h ${AOM_RTCD_CONFIG_FILE}
    OUTPUT_FILE ${AOM_RTCD_HEADER_FILE})
endforeach()

# Generate aom_version.h.
execute_process(COMMAND ${CMAKE_COMMAND}
                        -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
                        -DAOM_ROOT=${AOM_ROOT}
                        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                        -DPERL_EXECUTABLE=${PERL_EXECUTABLE} -P
                        "${AOM_ROOT}/build/cmake/version.cmake")
