# Copyright 2014 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22)

# Defer enabling C and CXX languages.
project(BoringSSL NONE)

# Don't install BoringSSL to system directories by default; it has no stable
# ABI. Instead, default to an "install" directory under the source.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install CACHE PATH "" FORCE)
endif()

include(cmake/go.cmake)
include(gen/sources.cmake)

enable_language(C)
enable_language(CXX)

# TODO(crbug,com/389897612): Register tests with CTest. For now, we include this
# only to pick up the standard |BUILD_TESTING| option.
include(CTest)
include(GNUInstallDirs)

set(INSTALL_ENABLED 1)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING AND
   BUILD_TESTING)
  find_package(PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules(LIBUNWIND libunwind-generic>=1.3.0)
    if(LIBUNWIND_FOUND)
      add_definitions(-DBORINGSSL_HAVE_LIBUNWIND)
    else()
      message("libunwind not found. Disabling unwind tests.")
    endif()
  else()
    message("pkgconfig not found. Disabling unwind tests.")
  endif()
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
if(NOT FIPS)
  if(CMAKE_BUILD_TYPE_LOWER STREQUAL "relwithassert" OR
     NOT CMAKE_BUILD_TYPE_LOWER MATCHES "rel")
    add_definitions(-DBORINGSSL_DISPATCH_TEST)
    # CMake automatically connects include_directories to the NASM
    # command-line, but not add_definitions.
    set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -DBORINGSSL_DISPATCH_TEST")
  endif()
endif()

# Add a RelWithAsserts build configuration. It is the same as Release, except it
# does not define NDEBUG, so asserts run.
foreach(VAR CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_ASM_FLAGS)
  string(REGEX REPLACE "(^| )[/-]DNDEBUG( |$)" " " "${VAR}_RELWITHASSERTS"
         "${${VAR}_RELEASE}")
endforeach()

if(BORINGSSL_PREFIX)
  add_definitions(-DBORINGSSL_PREFIX=${BORINGSSL_PREFIX})

  # CMake automatically connects include_directories to the NASM command-line,
  # but not add_definitions.
  set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -DBORINGSSL_PREFIX=${BORINGSSL_PREFIX}")
  set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -DBORINGSSL_PREFIX=${BORINGSSL_PREFIX}")
endif()

# Add the generated files to NASM's includes. With a final slash for NASM 2.
set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/gen/")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CLANG 1)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set some project-wide compiler flags.
if(CMAKE_COMPILER_IS_GNUCXX OR CLANG)
  set(C_CXX_FLAGS "-fno-strict-aliasing")
  if(NOT MSVC)
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
      # emscripten's emcc/clang does not accept the "-ggdb" flag.
      set(C_CXX_FLAGS "${C_CXX_FLAGS} -g")
    else()
      set(C_CXX_FLAGS "${C_CXX_FLAGS} -ggdb")
    endif()
    set(C_CXX_FLAGS "${C_CXX_FLAGS} -fno-common -fvisibility=hidden")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS}")
elseif(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -utf-8")
  # Without /Zc:__cplusplus, MSVC does not define the right value for
  # __cplusplus. See https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
  # If this becomes too problematic for downstream code, we can look at
  # _MSVC_LANG.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -utf-8 -Zc:__cplusplus")
endif()

if(WIN32)
  add_definitions(-D_HAS_EXCEPTIONS=0)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-DNOMINMAX)
  # Allow use of fopen.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# pthread_rwlock_t on Linux requires a feature flag. We limit this to Linux
# because, on Apple platforms, it instead disables APIs we use. See compat(5)
# and sys/cdefs.h. Reportedly, FreeBSD also breaks when this is set. See
# https://crbug.com/boringssl/471.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")
endif()

if(FUZZ)
  if(NOT CLANG)
    message(FATAL_ERROR "You need to build with Clang for fuzzing to work")
  endif()

  if(CMAKE_C_COMPILER_VERSION VERSION_LESS "6.0.0")
    message(FATAL_ERROR "You need Clang ≥ 6.0.0")
  endif()

  add_definitions(-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
  set(RUNNER_ARGS "-deterministic" "-fuzzer" "-shim-config" "fuzzer_mode.json")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,fuzzer-no-link -fsanitize-coverage=edge,indirect-calls")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,fuzzer-no-link -fsanitize-coverage=edge,indirect-calls")
endif()

if(BUILD_SHARED_LIBS)
  add_definitions(-DBORINGSSL_SHARED_LIBRARY)
  # Enable position-independent code globally. This is needed because
  # some library targets are OBJECT libraries.
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()

if(MSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable MSAN unless using Clang")
  endif()

  if(ASAN)
    message(FATAL_ERROR "ASAN and MSAN are mutually exclusive")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
  set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
endif()

if(ASAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable ASAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
endif()

if(CFI)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable CFI unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=cfi -fno-sanitize-trap=cfi -flto=thin")
  # We use Chromium's copy of clang, which requires -fuse-ld=lld if building
  # with -flto. That, in turn, can't handle -ggdb.
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
  string(REPLACE "-ggdb" "-g" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REPLACE "-ggdb" "-g" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # -flto causes object files to contain LLVM bitcode. Mixing those with
  # assembly output in the same static library breaks the linker.
  set(OPENSSL_NO_ASM "1")
endif()

if(TSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable TSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()

if(UBSAN)
  if(NOT CLANG)
    message(FATAL_ERROR "Cannot enable UBSAN unless using Clang")
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")

  if(NOT UBSAN_RECOVER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-sanitize-recover=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize-recover=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-sanitize-recover=undefined")
  endif()
endif()

if(GCOV)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if(FIPS)
  require_go()
  add_definitions(-DBORINGSSL_FIPS)
  if(FIPS_BREAK_TEST)
    add_definitions("-DBORINGSSL_FIPS_BREAK_${FIPS_BREAK_TEST}=1")
  endif()
  # The FIPS integrity check does not work for ASan and MSan builds.
  if(NOT ASAN AND NOT MSAN)
    if(BUILD_SHARED_LIBS)
      set(FIPS_SHARED "1")
    else()
      set(FIPS_DELOCATE "1")
    endif()
  endif()
  if(FIPS_SHARED)
    # The Android CMake files set -ffunction-sections and -fdata-sections,
    # which is incompatible with FIPS_SHARED.
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fno-function-sections -fno-data-sections")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fno-function-sections -fno-data-sections")
  endif()
endif()

if(OPENSSL_SMALL)
  add_definitions(-DOPENSSL_SMALL)
endif()

if(CONSTANT_TIME_VALIDATION)
  add_definitions(-DBORINGSSL_CONSTANT_TIME_VALIDATION)
endif()

if(MALLOC_FAILURE_TESTING)
  add_definitions(-DBORINGSSL_MALLOC_FAILURE_TESTING)
endif()

if(OPENSSL_NO_ASM)
  add_definitions(-DOPENSSL_NO_ASM)
endif()

if(FIPS_DELOCATE OR NOT OPENSSL_NO_ASM)
  # On x86 and x86_64 Windows, we use the NASM output.
  if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|amd64|x86|i[3-6]86")
    enable_language(ASM_NASM)
    set(OPENSSL_NASM TRUE)
    set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -gcv8")
  else()
    enable_language(ASM)
    if (NOT OPENSSL_NO_ASM)
      set(OPENSSL_ASM TRUE)
    endif()
    if(NOT WIN32)
      set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,--noexecstack")
    endif()
    # Clang's integrated assembler does not support debug symbols.
    if(NOT CMAKE_ASM_COMPILER_ID MATCHES "Clang")
      set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wa,-g")
    endif()
  endif()
endif()

if(OPENSSL_NO_SSE2_FOR_TESTING)
  add_definitions(-DOPENSSL_NO_SSE2_FOR_TESTING)
endif()

# Build a custom libc++. This must be the first target, so that it is linked to
# every subsequent target.
if(USE_CUSTOM_LIBCXX)
  if(NOT CLANG)
    message(FATAL_ERROR "USE_CUSTOM_LIBCXX only supported with Clang")
  endif()

  # CMake does not allow installing a library without installing dependencies.
  # If we installed libcrypto, we'd have to install our custom libc++, which
  # does not make sense. As this is a test-only configuration, disable
  # installing.
  set(INSTALL_ENABLED 0)

  # CMAKE_CXX_FLAGS ends up in the linker flags as well, so use
  # add_compile_options. There does not appear to be a way to set
  # language-specific compile-only flags.
  add_compile_options("-nostdinc++")
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -nostdlib++")
  include_directories(
    SYSTEM
    util/bot/libcxx-config
    util/bot/libcxx/include
    util/bot/libcxxabi/include
  )

  # This is patterned after buildtools/third_party/libc++/BUILD.gn and
  # buildtools/third_party/libc++abi/BUILD.gn in Chromium.

  file(GLOB LIBCXX_SOURCES "util/bot/libcxx/src/*.cpp")
  file(GLOB LIBCXXABI_SOURCES "util/bot/libcxxabi/src/*.cpp")

  # This file is meant for exception-less builds.
  list(REMOVE_ITEM LIBCXXABI_SOURCES "trunk/src/cxa_noexception.cpp")
  # libc++ also defines new and delete.
  list(REMOVE_ITEM LIBCXXABI_SOURCES "trunk/src/stdlib_new_delete.cpp")
  if(TSAN)
    # ThreadSanitizer tries to intercept these symbols. Skip them to avoid
    # symbol conflicts.
    list(REMOVE_ITEM LIBCXXABI_SOURCES "trunk/src/cxa_guard.cpp")
  endif()

  add_library(libcxxabi ${LIBCXXABI_SOURCES})
  target_compile_definitions(
    libcxxabi PRIVATE
    -D_LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS
  )

  add_library(libcxx ${LIBCXX_SOURCES})
  if(ASAN OR MSAN OR TSAN)
    # Sanitizers try to intercept new and delete.
    target_compile_definitions(
      libcxx PRIVATE
      -D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS
    )
  endif()
  target_compile_definitions(
    libcxx PRIVATE
    -D_LIBCPP_BUILDING_LIBRARY
    -DLIBCXX_BUILDING_LIBCXXABI
    -DLIBC_NAMESPACE=bssl_llvm_libc
  )
  # LLVM libc++ depends on some shared headers from LLVM libc.
  target_include_directories(libcxx PRIVATE util/bot/llvm-libc)
  set_target_properties(
    libcxx libcxxabi PROPERTIES
    # libc++ and libc++abi must be built in C++23 mode.
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED TRUE
  )
  # libc++abi depends on libc++ internal headers.
  set_property(TARGET libcxx libcxxabi APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/util/bot/libcxx/src")
  target_link_libraries(libcxx libcxxabi)

  # Link all subsequent libraries to the custom libc++.
  link_libraries(libcxx)

  # Third-party CMake files that export targets will this target itself be
  # exported. This is not meant to be imported, as USE_CUSTOM_LIBCXX is a
  # test-only configuration.
  export(TARGETS libcxx libcxxabi NAMESPACE bssl_test_only
         FILE ${PROJECT_BINARY_DIR}/bssl_test_only.cmake)
endif()

if(BUILD_TESTING)
  # Add minimal googletest targets. The provided one has many side-effects, and
  # googletest has a very straightforward build.
  add_library(
    boringssl_gtest
    STATIC
    third_party/googletest/googlemock/src/gmock-all.cc
    third_party/googletest/googletest/src/gtest-all.cc
  )
  target_include_directories(
      boringssl_gtest
      PUBLIC
      third_party/googletest/googlemock/include
      third_party/googletest/googletest/include
      PRIVATE
      third_party/googletest/googlemock
      third_party/googletest/googletest
  )

  # Wrap this in a function for variable scoping. Switch to block() when our
  # minimum CMake is at least CMake 3.25.
  function(add_google_benchmark)
    set(GOOGLETEST_PATH "${PROJECT_SOURCE_DIR}/third_party/googletest")
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF)
    set(BENCHMARK_ENABLE_TESTING OFF)
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(BENCHMARK_INSTALL_DOCS OFF)
    if(USE_CUSTOM_LIBCXX)
      # benchmark's feature testing does not pick up the custom STL.
      set(HAVE_STD_REGEX 1)
    endif()
    if(DEFINED CMAKE_OSX_SYSROOT)
      # CMAKE_CROSSCOMPILING is not correctly set when using CMAKE_OSX_SYSROOT
      # to specify iOS vs macOS. Set CMAKE_CROSSCOMPILING so benchmark feature
      # detection does not try to run its test programs.
      set(CMAKE_CROSSCOMPILING 1)
    endif()
    add_subdirectory(third_party/benchmark)
  endfunction()
  add_google_benchmark()
endif()

if(FUZZ AND LIBFUZZER_FROM_DEPS)
  file(GLOB LIBFUZZER_SOURCES "util/bot/libFuzzer/*.cpp")
  add_library(Fuzzer STATIC ${LIBFUZZER_SOURCES})
  # libFuzzer must be built without -fsanitize-coverage options or clang
  # crashes.
  set_target_properties(
    Fuzzer PROPERTIES
    COMPILE_FLAGS "-fsanitize-coverage=0"
  )
endif()

# Enable a suite of warnings. We do this here to avoid applying to external
# dependencies, built above. Note this uses the list-based add_compile_options,
# instead of CMAKE_*_FLAGS (string-based). Changes to CMAKE_*_FLAGS still apply
# to earlier-defined targets.
#
# TODO(crbug.com/389897612): Should these be set on a per-target basis?
if(CMAKE_COMPILER_IS_GNUCXX OR CLANG)
  set(C_CXX_WARNINGS -Werror -Wformat=2 -Wmissing-field-initializers -Wshadow
      -Wsign-compare -Wtype-limits -Wvla -Wwrite-strings -Wimplicit-fallthrough)
  set(C_WARNINGS -Wold-style-definition -Wstrict-prototypes)
  set(CXX_WARNING -Wnon-virtual-dtor)

  # clang-cl sets both CLANG and MSVC. It implements clang's diagnostics, but
  # some options behave as in MSVC.
  if(MSVC)
    # clang-cl sets different default warnings than clang. It also treats -Wall
    # as -Weverything, to match MSVC. Instead -W3 is the alias for -Wall.
    # See http://llvm.org/viewvc/llvm-project?view=revision&revision=319116
    list(APPEND C_CXX_WARNINGS -W3 -Wno-unused-parameter)
  else()
    list(APPEND C_CXX_WARNINGS -Wall)
  endif()

  if(CLANG)
    list(APPEND C_CXX_WARNINGS -Wnewline-eof -Wextra-semi -fcolor-diagnostics
         -Wheader-hygiene)
  else()
    list(APPEND C_CXX_WARNINGS -Wformat-signedness)
    # GCC (at least 4.8.4) has a bug where it'll find unreachable free() calls
    # and declare that the code is trying to free a stack pointer.
    list(APPEND C_CXX_WARNINGS -Wno-free-nonheap-object)
  endif()

  # In GCC, -Wmissing-declarations is the C++ spelling of -Wmissing-prototypes
  # and using the wrong one is an error. In Clang, -Wmissing-prototypes is the
  # spelling for both and -Wmissing-declarations is some other warning.
  #
  # https://gcc.gnu.org/onlinedocs/gcc-7.1.0/gcc/Warning-Options.html#Warning-Options
  # https://clang.llvm.org/docs/DiagnosticsReference.html#wmissing-prototypes
  # https://clang.llvm.org/docs/DiagnosticsReference.html#wmissing-declarations
  if(CLANG)
    list(APPEND C_CXX_WARNINGS -Wmissing-prototypes)
  else()
    list(APPEND C_WARNINGS -Wmissing-prototypes)
    list(APPEND CXX_WARNINGS -Wmissing-declarations)
  endif()

  # -Wstring-concatenation was added in Clang 12.0.0, which corresponds to
  # AppleClang 13.0.0 per the table in
  # https://en.wikipedia.org/wiki/Xcode#Toolchain_versions
  if((CMAKE_C_COMPILER_ID STREQUAL "Clang" AND
      CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0.0") OR
     (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND
      CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "13.0.0"))
    list(APPEND C_CXX_WARNINGS -Wstring-concatenation)
  endif()

  # Clang 12's -Wframe-larger-than reportedly does not work in clang-cl. See
  # https://crbug.com/42290587. Clang 13 includes the following fix, which may
  # be related. Speculatively gate on Clang 13. That corresponds to
  # AppleClang 13.1.6.
  # https://github.com/llvm/llvm-project/commit/6aaf4fa2885600b0e31042071ad06f78218ab0f2
  #
  # Only enable this warning in release builds.
  if(CMAKE_BUILD_TYPE MATCHES "[rR][eE][lL]" AND
     ((CMAKE_C_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "13.0.0") OR
      (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND
       CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "13.1.6")))
    list(APPEND C_CXX_WARNINGS -Wframe-larger-than=25344)
  endif()

  # -Wctad-maybe-unsupported was added in Clang 10, which is AppleClang 12.0.0.
  if((CMAKE_C_COMPILER_ID STREQUAL "Clang" AND
      CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "10.0.0") OR
     (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND
      CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0.0"))
    list(APPEND C_CXX_WARNINGS -Wctad-maybe-unsupported)
  endif()

  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX>:${C_CXX_WARNINGS}>"
    "$<$<COMPILE_LANGUAGE:C>:${C_WARNINGS}>"
    "$<$<COMPILE_LANGUAGE:CXX>:${CXX_WARNINGS}>"
  )
elseif(MSVC)
  set(MSVC_DISABLED_WARNINGS_LIST
      "-wd4100" # 'exarg' : unreferenced formal parameter
      "-wd4127" # conditional expression is constant
      "-wd4244" # 'function' : conversion from 'int' to 'uint8_t',
                # possible loss of data
      "-wd4267" # conversion from 'size_t' to 'int', possible loss of data
      "-wd4702" # unreachable code; MSVC's warning is too aggressive. See
                # https://crbug.com/385161043
      "-wd4706" # assignment within conditional expression
      )
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX>:-W4;-WX;${MSVC_DISABLED_WARNINGS_LIST}>")
endif()

if(OPENSSL_ASM)
  set(CRYPTO_SOURCES_ASM_USED ${CRYPTO_SOURCES_ASM})
  set(BCM_SOURCES_ASM_USED ${BCM_SOURCES_ASM})
  set(TEST_SUPPORT_SOURCES_ASM_USED ${TEST_SUPPORT_SOURCES_ASM})
elseif(OPENSSL_NASM)
  set(CRYPTO_SOURCES_ASM_USED ${CRYPTO_SOURCES_NASM})
  set(BCM_SOURCES_ASM_USED ${BCM_SOURCES_NASM})
  set(TEST_SUPPORT_SOURCES_ASM_USED ${TEST_SUPPORT_SOURCES_NASM})
endif()

if(FIPS_DELOCATE AND FIPS_SHARED)
  message(FATAL_ERROR "Can't set both delocate and shared mode for FIPS build")
endif()

# OSS-Fuzz relies on BORINGSSL_ALLOW_CXX_RUNTIME because -fno-rtti and
# -fsanitize=vptr are incompatible.
set(NO_CXX_RUNTIME_FLAGS)
if(NOT MSVC AND NOT BORINGSSL_ALLOW_CXX_RUNTIME)
  # Without -fno-exceptions, use of std::unique_ptr emits a call to
  # std::terminate.
  set(NO_CXX_RUNTIME_FLAGS -fno-exceptions -fno-rtti)
endif()

if(FIPS_DELOCATE)
  add_library(bcm_c_generated_asm STATIC ${BCM_SOURCES})
  # The C++ code in libcrypto shouldn't depend on libstdc++.
  target_compile_options(bcm_c_generated_asm PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${NO_CXX_RUNTIME_FLAGS}> "-S")
  target_include_directories(bcm_c_generated_asm PRIVATE ${PROJECT_SOURCE_DIR}/include)
  set_target_properties(bcm_c_generated_asm PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_compile_definitions(bcm_c_generated_asm PRIVATE -DBORINGSSL_IMPLEMENTATION)
  if(CLANG)
    # Clang warns when passing both -c (from CMake) and -S.
    target_compile_options(bcm_c_generated_asm PRIVATE "-Wno-unused-command-line-argument")
  endif()
  if(CFI)
    # Delocate needs real assembly code so we need to disable CFI and LTO in the FIPS module.
    target_compile_options(bcm_c_generated_asm PRIVATE "-fno-lto" "-fno-sanitize=cfi")
  endif()

  set(TARGET_FLAG "")
  if(CMAKE_ASM_COMPILER_TARGET)
    set(TARGET_FLAG "--target=${CMAKE_ASM_COMPILER_TARGET}")
  endif()

  go_executable(delocate boringssl.googlesource.com/boringssl.git/util/fipstools/delocate)
  add_custom_command(
    OUTPUT bcm-delocated.S
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/delocate
            -a $<TARGET_FILE:bcm_c_generated_asm>
            -o ${CMAKE_CURRENT_BINARY_DIR}/bcm-delocated.S
            -cc ${CMAKE_ASM_COMPILER}
            -cc-flags "${TARGET_FLAG} ${CMAKE_ASM_FLAGS}"
            ${BCM_SOURCES_ASM_USED}
            ${CRYPTO_HEADERS}
    DEPENDS bcm_c_generated_asm
            delocate
            ${BCM_SOURCES_ASM_USED}
            ${CRYPTO_HEADERS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  add_library(bcm_hashunset OBJECT bcm-delocated.S)

  # bcm_hashunset may still have relocations, just link-independent ones. Link a
  # sample shared object before computing the hash.
  add_library(bcm_relocated SHARED $<TARGET_OBJECTS:bcm_hashunset>)
  set_target_properties(bcm_relocated PROPERTIES LINKER_LANGUAGE CXX)
  target_link_options(bcm_relocated PRIVATE "-Wl,-z,undefs")

  go_executable(inject_hash
                boringssl.googlesource.com/boringssl.git/util/fipstools/inject_hash)
  add_custom_command(
    OUTPUT bcm.o
    COMMAND ./inject_hash -o bcm.o -in-object $<TARGET_OBJECTS:bcm_hashunset>
            -in-hash $<TARGET_FILE:bcm_relocated>
    DEPENDS bcm_hashunset bcm_relocated inject_hash
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  set(CRYPTO_FIPS_OBJECTS bcm.o)
elseif(FIPS_SHARED)
  if(NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR "FIPS_SHARED set but not BUILD_SHARED_LIBS")
  endif()

  add_library(bcm_library STATIC ${BCM_SOURCES} ${BCM_SOURCES_ASM_USED})
  # The C++ code in libcrypto shouldn't depend on libstdc++.
  target_compile_options(bcm_library PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${NO_CXX_RUNTIME_FLAGS}>)
  target_include_directories(bcm_library PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_compile_definitions(bcm_library PRIVATE -DBORINGSSL_IMPLEMENTATION)

  add_custom_command(
    OUTPUT bcm.o
    COMMAND ${CMAKE_LINKER} -r -T ${CMAKE_CURRENT_SOURCE_DIR}/crypto/fipsmodule/fips_shared.lds -o bcm.o --whole-archive $<TARGET_FILE:bcm_library>
    DEPENDS bcm_library crypto/fipsmodule/fips_shared.lds
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  set(CRYPTO_FIPS_OBJECTS bcm.o)
else()
  add_library(fipsmodule OBJECT ${BCM_SOURCES} ${BCM_SOURCES_ASM_USED})
  # The C++ code in libcrypto shouldn't depend on libstdc++.
  target_compile_options(fipsmodule PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${NO_CXX_RUNTIME_FLAGS}>)
  target_include_directories(fipsmodule PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_compile_definitions(fipsmodule PRIVATE -DBORINGSSL_IMPLEMENTATION)
  set(CRYPTO_FIPS_OBJECTS $<TARGET_OBJECTS:fipsmodule>)
endif()

add_library(crypto ${CRYPTO_SOURCES} ${CRYPTO_FIPS_OBJECTS} ${CRYPTO_SOURCES_ASM_USED})
  # The C++ code in libcrypto shouldn't depend on libstdc++.
target_compile_options(crypto PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${NO_CXX_RUNTIME_FLAGS}>)
target_include_directories(crypto PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(crypto PRIVATE -DBORINGSSL_IMPLEMENTATION)
set_property(TARGET crypto PROPERTY EXPORT_NAME Crypto)

if(FIPS_SHARED)
  # Rewrite libcrypto.so to inject the correct module hash value. This assumes
  # UNIX-style library naming, but we only support FIPS mode on Linux anyway.
  add_custom_command(
    TARGET crypto POST_BUILD
    COMMAND ${GO_EXECUTABLE} run
    ${CMAKE_CURRENT_SOURCE_DIR}/util/fipstools/inject_hash/inject_hash.go
    -o libcrypto.so -in-object libcrypto.so
    # The DEPENDS argument to a POST_BUILD rule appears to be ignored. Thus
    # go_executable isn't used (as it doesn't get built), but we list this
    # dependency anyway in case it starts working in some CMake version.
    DEPENDS util/fipstools/inject_hash/inject_hash.go
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
endif()

if(WIN32)
  target_link_libraries(crypto ws2_32)
endif()

# CMAKE_SYSTEM_NAME is "Generic" for embedded OSes:
# https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html#toolchain-files
#
# For now we assume embedded OSes do not have threads. Additionally, the Threads
# package does not work with Android, but Android does not require any extra
# parameters to link pthreads.
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Generic|Android)$")
  find_package(Threads REQUIRED)
  target_link_libraries(crypto Threads::Threads)
endif()

add_library(ssl ${SSL_SOURCES})
# Although libssl also provides headers that require an include directory, the
# flag is already specified by libcrypto, so we omit target_include_directories
# here.
set_property(TARGET ssl PROPERTY EXPORT_NAME SSL)
target_link_libraries(ssl crypto)
target_compile_definitions(ssl PRIVATE -DBORINGSSL_IMPLEMENTATION)

add_library(decrepit ${DECREPIT_SOURCES})
target_link_libraries(decrepit ssl crypto)
target_compile_definitions(decrepit PRIVATE -DBORINGSSL_IMPLEMENTATION)

if(APPLE)
  set(PKI_CXX_FLAGS "-fno-aligned-new")
endif()
add_library(pki ${PKI_SOURCES})
target_link_libraries(pki crypto)
target_compile_options(pki PRIVATE ${PKI_CXX_FLAGS})
target_compile_definitions(pki PRIVATE -DBORINGSSL_IMPLEMENTATION)
if(MSVC)
  # libpki has a DLL C++ interface, where this warning is expected:
  set(PKI_MSVC_DISABLED_WARNINGS_LIST
      "-wd4251" # assignment within conditional expression
      )
  target_compile_options(pki PUBLIC
    "$<$<COMPILE_LANGUAGE:C,CXX>:${PKI_MSVC_DISABLED_WARNINGS_LIST}>")
endif()

if(BUILD_TESTING)
  add_library(test_support_lib STATIC
              ${TEST_SUPPORT_SOURCES} ${TEST_SUPPORT_SOURCES_ASM_USED})
  if(LIBUNWIND_FOUND)
    target_compile_options(test_support_lib PRIVATE ${LIBUNWIND_CFLAGS_OTHER})
    target_include_directories(test_support_lib PRIVATE ${LIBUNWIND_INCLUDE_DIRS})
    target_link_libraries(test_support_lib ${LIBUNWIND_LDFLAGS})
  endif()
  if(WIN32)
    target_link_libraries(test_support_lib dbghelp)
  endif()
  target_link_libraries(test_support_lib boringssl_gtest crypto)

  # Declare a dummy target to build all unit tests. Test targets should inject
  # themselves as dependencies next to the target definition.
  add_custom_target(boringssl_all_tests)

  # TODO(crbug.com/42290412): Drive this build with build.json and flatten.
  add_subdirectory(ssl/test)

  # urandom_test is a separate binary because it needs to be able to observe the
  # PRNG initialisation, which means that it can't have other tests running before
  # it does.
  add_executable(urandom_test ${URANDOM_TEST_SOURCES})
  target_link_libraries(urandom_test test_support_lib boringssl_gtest crypto)
  add_dependencies(boringssl_all_tests urandom_test)

  add_executable(crypto_test ${CRYPTO_TEST_SOURCES})
  target_link_libraries(crypto_test test_support_lib boringssl_gtest crypto)
  add_dependencies(boringssl_all_tests crypto_test)

  add_executable(ssl_test ${SSL_TEST_SOURCES})
  target_link_libraries(ssl_test test_support_lib boringssl_gtest ssl crypto)
  add_dependencies(boringssl_all_tests ssl_test)

  add_executable(decrepit_test ${DECREPIT_TEST_SOURCES})
  target_link_libraries(decrepit_test test_support_lib boringssl_gtest
                        decrepit ssl crypto)
  add_dependencies(boringssl_all_tests decrepit_test)

  add_executable(pki_test ${PKI_TEST_SOURCES})
  target_link_libraries(pki_test test_support_lib boringssl_gtest pki crypto)
  target_compile_options(pki_test PRIVATE ${PKI_CXX_FLAGS})
  add_dependencies(boringssl_all_tests pki_test)

  add_executable(test_fips util/fipstools/test_fips.cc)
  target_link_libraries(test_fips crypto)

  add_executable(jitter_deltas util/fipstools/entropy/jitter_deltas.cc)
  target_link_libraries(jitter_deltas crypto)

  add_executable(bssl_bench ${BENCH_SOURCES})
  target_link_libraries(bssl_bench ssl pki crypto benchmark::benchmark)
endif()

if(FIPS)
  add_executable(modulewrapper ${MODULEWRAPPER_SOURCES})
  target_link_libraries(modulewrapper crypto)
  add_executable(entropy_modulewrapper ${ENTROPY_MODULEWRAPPER_SOURCES})
  target_link_libraries(entropy_modulewrapper crypto)
endif()

add_executable(bssl ${BSSL_SOURCES})
target_link_libraries(bssl ssl crypto)

if(BUILD_TESTING)
  add_executable(generate_mldsa_certs pki/testdata/verify_unittest/generate_mldsa_certs.cc)
  target_link_libraries(generate_mldsa_certs crypto)
endif()

if(FUZZ)
  # TODO(crbug.com/42290412): Drive this build with build.json and flatten.
  add_subdirectory(fuzz)
endif()

if(RUST_BINDINGS)
  find_program(BINDGEN_EXECUTABLE bindgen REQUIRED)
  add_subdirectory(rust)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(HANDSHAKER_ARGS "-handshaker-path" $<TARGET_FILE:handshaker>)
endif()

if(BUILD_TESTING)
  if(FIPS)
    add_custom_target(
      acvp_tests
      COMMAND ${GO_EXECUTABLE} build -o ${CMAKE_CURRENT_BINARY_DIR}/acvptool
              boringssl.googlesource.com/boringssl.git/util/fipstools/acvp/acvptool
      COMMAND ${GO_EXECUTABLE} build -o ${CMAKE_CURRENT_BINARY_DIR}/testmodulewrapper
              boringssl.googlesource.com/boringssl.git/util/fipstools/acvp/acvptool/testmodulewrapper
      COMMAND cd util/fipstools/acvp/acvptool/test &&
              ${GO_EXECUTABLE} run check_expected.go
              -tool ${CMAKE_CURRENT_BINARY_DIR}/acvptool
              -module-wrappers modulewrapper:$<TARGET_FILE:modulewrapper>,testmodulewrapper:${CMAKE_CURRENT_BINARY_DIR}/testmodulewrapper
              -tests tests.json
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS modulewrapper
      USES_TERMINAL)

    add_custom_target(
      fips_specific_tests_if_any
      DEPENDS acvp_tests
    )
  else()
    add_custom_target(fips_specific_tests_if_any)
  endif()

  file(STRINGS util/go_tests.txt GO_TESTS)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
              util/go_tests.txt)

  # CMake target names can clash when projects are imported with
  # add_subdirectory. Only emit the run_tests target if we're the top-level
  # target. See https://crbug.com/428112180.
  #
  # Also disable on Visual Studio for now due to name clash.
  # See https://crbug.com/465077378.
  if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_GENERATOR MATCHES "^Visual Studio ")
    if(GO_EXECUTABLE)
      add_custom_target(
          run_unit_tests
          COMMAND ${CMAKE_COMMAND} -E echo
          COMMAND ${CMAKE_COMMAND} -E echo "Checking pregenerated files"
          COMMAND ${GO_EXECUTABLE} run ./util/pregenerate -check
          COMMAND ${CMAKE_COMMAND} -E echo
          COMMAND ${CMAKE_COMMAND} -E echo "Running Go tests"
          COMMAND ${GO_EXECUTABLE} test ${GO_TESTS}
          COMMAND ${CMAKE_COMMAND} -E echo
          COMMAND ${CMAKE_COMMAND} -E echo "Running unit tests"
          COMMAND ${GO_EXECUTABLE} run util/all_tests.go -build-dir
                  ${CMAKE_CURRENT_BINARY_DIR}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          DEPENDS boringssl_all_tests fips_specific_tests_if_any
          USES_TERMINAL)
      add_custom_target(
          run_tests
          COMMAND ${CMAKE_COMMAND} -E echo
          COMMAND ${CMAKE_COMMAND} -E echo "Running SSL tests"
          COMMAND cd ssl/test/runner &&
                  ${GO_EXECUTABLE} test -shim-path $<TARGET_FILE:bssl_shim>
                    ${HANDSHAKER_ARGS} ${RUNNER_ARGS}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          DEPENDS run_unit_tests bssl_shim handshaker
          USES_TERMINAL)
    else()
      add_custom_target(
          run_unit_tests
          COMMAND ${CMAKE_COMMAND} -E echo "Running tests requires Go"
          COMMAND ${CMAKE_COMMAND} -E false)
      add_custom_target(
          run_tests
          COMMAND ${CMAKE_COMMAND} -E echo "Running tests requires Go"
          COMMAND ${CMAKE_COMMAND} -E false)
    endif()
  endif()
endif()

if(BORINGSSL_PREFIX)
  require_go()
  add_custom_target(
    verify_boringssl_prefix ALL
    COMMENT "Verifying symbol prefixing"
    COMMAND ${GO_EXECUTABLE}
            run ${CMAKE_CURRENT_SOURCE_DIR}/util/audit_symbols.go
            -ignore-symbols-with ${BORINGSSL_PREFIX}
            $<TARGET_FILE:crypto>
    DEPENDS crypto
    USES_TERMINAL
  )
endif()

if(INSTALL_ENABLED)
  install(TARGETS crypto ssl EXPORT OpenSSLTargets)
  install(TARGETS bssl)
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT OpenSSLTargets
          FILE OpenSSLTargets.cmake
          NAMESPACE OpenSSL::
          DESTINATION lib/cmake/OpenSSL)
  install(FILES cmake/OpenSSLConfig.cmake DESTINATION lib/cmake/OpenSSL)
endif()
