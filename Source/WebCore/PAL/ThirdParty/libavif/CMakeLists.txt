# Copyright 2019 Joe Drago. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.13)

# New in CMake version 3.15. MSVC warning flags are not in CMAKE_<LANG>_FLAGS by default.
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif()

# The root directory of the avif source
set(AVIF_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Specify search path for CMake modules to be loaded by include() and find_package()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

project(libavif LANGUAGES C VERSION 1.0.3)

# Set C99 as the default
set(CMAKE_C_STANDARD 99)

# SOVERSION scheme: MAJOR.MINOR.PATCH
#   If there was an incompatible interface change:
#     Increment MAJOR. Set MINOR and PATCH to 0
#   If there was a compatible interface change:
#     Increment MINOR. Set PATCH to 0
#   If the source code was changed, but there were no interface changes:
#     Increment PATCH.
set(LIBRARY_VERSION_MAJOR 16)
set(LIBRARY_VERSION_MINOR 0)
set(LIBRARY_VERSION_PATCH 3)
set(LIBRARY_VERSION "${LIBRARY_VERSION_MAJOR}.${LIBRARY_VERSION_MINOR}.${LIBRARY_VERSION_PATCH}")
set(LIBRARY_SOVERSION ${LIBRARY_VERSION_MAJOR})

option(BUILD_SHARED_LIBS "Build shared avif library" ON)

option(AVIF_ENABLE_WERROR "Treat all compiler warnings as errors" OFF)
option(AVIF_ENABLE_WUNUSED_RESULT "Add [[nodiscard]] to some functions. CMake must be at least 3.21 to force C23" OFF)

option(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R "Enable experimental YCgCo-R matrix code" OFF)
option(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP
       "Enable experimental gain map code (for HDR images that look good both on HDR and SDR displays)" OFF
)
option(AVIF_ENABLE_EXPERIMENTAL_AVIR "Enable experimental reduced header" OFF)

set(AVIF_PKG_CONFIG_EXTRA_LIBS_PRIVATE "")
set(AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE "")

function(set_local_or_system_option VAR DEFAULT TEXT)
    # Deal with the older way of setting options.
    if(DEFINED AVIF_LOCAL_${VAR})
        if(AVIF_LOCAL_${VAR})
            set(DEFAULT "LOCAL")
        else()
            set(DEFAULT "SYSTEM")
        endif()
    elseif(DEFINED AVIF_${VAR})
        set(DEFAULT ${AVIF_${VAR}})
    endif()
    set(AVIF_${VAR} ${DEFAULT} CACHE STRING ${TEXT} FORCE)
    set_property(CACHE AVIF_${VAR} PROPERTY STRINGS OFF LOCAL SYSTEM)
endfunction()
function(set_codec_option CODEC NAME ENCDEC EXTRA)
    if(DEFINED AVIF_CODEC_${CODEC})
        set(DEFAULT ${AVIF_CODEC_${CODEC}})
        # Deal with the older way of setting options.
        if(AVIF_CODEC_${CODEC} STREQUAL "ON")
            if(AVIF_LOCAL_${CODEC})
                set(DEFAULT "LOCAL")
            else()
                set(DEFAULT "SYSTEM")
            endif()
            message(WARNING "Setting AVIF_CODEC_${CODEC} and AVIF_LOCAL_${CODEC} is deprecated. "
                            "Set AVIF_CODEC_${CODEC} to ${DEFAULT} instead."
            )
        endif()
    else()
        set(DEFAULT "OFF")
    endif()
    if(AVIF_CODEC_${CODEC} STREQUAL "OFF" AND AVIF_LOCAL_${CODEC} STREQUAL "OFF")
        message(WARNING "Setting AVIF_LOCAL_${CODEC} is deprecated. " "Only set AVIF_CODEC_${CODEC} to OFF instead.")
    endif()
    set(AVIF_CODEC_${CODEC} ${DEFAULT} CACHE STRING "Use the ${NAME} codec for ${ENCDEC}${EXTRA}" FORCE)
    set_property(CACHE AVIF_CODEC_${CODEC} PROPERTY STRINGS OFF LOCAL SYSTEM)
endfunction()
set_codec_option(AOM "AOM" "encoding/decoding" " (see AVIF_CODEC_AOM_DECODE/AVIF_CODEC_AOM_ENCODE)")
set_codec_option(DAV1D "dav1d" "decoding" "")
set_codec_option(LIBGAV1 "libgav1" "decoding" "")
set_codec_option(RAV1E "rav1e" "encoding" "")
set_codec_option(SVT "SVT-AV1" "encoding" "")
set_codec_option(AVM "AVM (AV2)" "encoding/decoding" " (EXPERIMENTAL)")

# These options allow libavif to only link against / use libaom's encoder or decoder, instead of being forced to use both
include(CMakeDependentOption)
cmake_dependent_option(
    AVIF_CODEC_AOM_DECODE "if AVIF_CODEC_AOM is on, use/offer libaom's decoder" ON "NOT AVIF_CODEC_AOM STREQUAL OFF" OFF
)
cmake_dependent_option(
    AVIF_CODEC_AOM_ENCODE "if AVIF_CODEC_AOM is on, use/offer libaom's encoder" ON "NOT AVIF_CODEC_AOM STREQUAL OFF" OFF
)

option(AVIF_LOCAL_GTEST
       "Build the GoogleTest framework by providing your own copy of the repo in ext/googletest (see Local Builds in README)" OFF
)
option(AVIF_LOCAL_FUZZTEST
       "Build the Google FuzzTest framework by providing your own copy of the repo in ext/fuzztest (see Local Builds in README)"
       OFF
)

option(
    AVIF_ENABLE_COMPLIANCE_WARDEN
    "Check all avifEncoderFinish() output for AVIF specification compliance. Depends on gpac/ComplianceWarden which can be added with ext/compliance_warden.sh"
    OFF
)

set(AVIF_USE_CXX OFF)

if(AVIF_BUILD_APPS
   OR AVIF_ENABLE_FUZZTEST
   OR AVIF_ENABLE_GTEST
   OR AVIF_CODEC_LIBGAV1 STREQUAL "LOCAL"
   OR AVIF_CODEC_LIBGAV1 STREQUAL "SYSTEM"
)
    set(AVIF_USE_CXX ON)
endif()

if(APPLE)
    set(XCRUN xcrun)
else()
    set(XCRUN)
endif()

# This is also needed to get shared libraries (e.g. pixbufloader-avif) to compile against a static libavif.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(BUILD_SHARED_LIBS)
    set(AVIF_LIBRARY_PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
else()
    set(AVIF_LIBRARY_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
endif()

add_library(avif_obj OBJECT)
add_library(avif)

# Adds <target> to avif_obj's public link libraries for build, and adds
# the <target> library as an install link library for export if a consumer
# would need to include that library alongside libavif when linking.
function(avif_target_link_library target)
    target_link_libraries(avif_obj PUBLIC $<BUILD_INTERFACE:${target}>)
    get_target_property(target_is_local ${target} AVIF_LOCAL)
    if(target_is_local)
        return()
    endif()
    get_target_property(install_target ${target} IMPORTED_SONAME)
    if(NOT install_target)
        set(install_target ${target})
    endif()
    # The transitive dependency is an export link library if it is a shared
    # library, or if it is a static library in a static build
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "SHARED_LIBRARY" OR NOT BUILD_SHARED_LIBS)
        target_link_libraries(avif PUBLIC $<INSTALL_INTERFACE:${install_target}>)
    endif()
endfunction()

#[[
check_avif_option(<option> TARGET <target> PKG_NAME <PackageName>)

If <option> is equal to "SYSTEM", uses <target> if it already exists, otherwise calls find_package(<PackageName>). If <option>
is "LOCAL", includes Local<PackageName>.cmake. Sets <option>_ENABLED to ON if the option is enabled and the target is usable.
]]
macro(check_avif_option _VAR)
    set(_oneValueArgs TARGET PKG_NAME)
    cmake_parse_arguments(_AVIF_OPTION "" "${_oneValueArgs}" "" ${ARGN})
    string(SUBSTRING ${_AVIF_OPTION_PKG_NAME} 0 1 FIRST_LETTER)
    string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
    string(REGEX REPLACE "^.(.*)" "Local${FIRST_LETTER}\\1" _LOCAL_INCLUDE "${_AVIF_OPTION_PKG_NAME}")
    set(${_VAR}_ENABLED OFF)
    if(${_VAR} STREQUAL "LOCAL" OR ${_VAR} STREQUAL "SYSTEM")
        if(${_VAR} STREQUAL "LOCAL" AND TARGET ${_AVIF_OPTION_TARGET})
            message(ERROR "${_AVIF_OPTION_TARGET} is already defined and ${_VAR} should be set to SYSTEM to use it")
            return()
        endif()
        set(${_VAR}_ENABLED ON)
        if(NOT TARGET ${_AVIF_OPTION_TARGET})
            if(${_VAR} STREQUAL "LOCAL")
                include(${_LOCAL_INCLUDE})
            elseif(${_VAR} STREQUAL "SYSTEM")
                find_package(${_AVIF_OPTION_PKG_NAME} REQUIRED)
            endif()
        endif()
    endif()
endmacro()

set_local_or_system_option("ZLIBPNG" "SYSTEM" "Use zlib and libpng.")
if(AVIF_ZLIBPNG STREQUAL "LOCAL")
    include(LocalZlibpng)
endif()

set_local_or_system_option("JPEG" "SYSTEM" "Use jpeg.")
if(AVIF_JPEG STREQUAL "LOCAL")
    include(LocalJpeg)
endif()

set_local_or_system_option("LIBYUV" "OFF" "Use libyuv.")
check_avif_option(AVIF_LIBYUV TARGET yuv::yuv PKG_NAME libyuv)
if(AVIF_LIBYUV_ENABLED)
    # libyuv 1755 exposed all of the I*Matrix() functions, which libavif relies on.
    # libyuv 1774 exposed ScalePlane_12 function, which libavif can use for some additional optimizations.
    # libyuv 1813 added the I*ToARGBMatrixFilter() functions, which libavif can use with the bilinear filter.
    if(NOT LIBYUV_VERSION)
        message(STATUS "libavif: libyuv found, but version unknown; libyuv-based fast paths disabled.")
        unset(AVIF_LIBYUV_ENABLED)
    elseif(LIBYUV_VERSION LESS 1755)
        message(STATUS "libavif: libyuv (${LIBYUV_VERSION}) found, but is too old; libyuv-based fast paths disabled.")
        unset(AVIF_LIBYUV_ENABLED)
    else()
        message(STATUS "libavif: libyuv (${LIBYUV_VERSION}) found; libyuv-based fast paths enabled.")
        if(LIBYUV_VERSION LESS 1813)
            message(STATUS "libavif: some libyuv optimizations require at least version 1813 to work.")
        endif()
    endif()
endif()
if(AVIF_LIBYUV_ENABLED)
    target_compile_definitions(avif_obj PRIVATE -DAVIF_LIBYUV_ENABLED=1)
    avif_target_link_library(yuv::yuv)
    set(AVIF_PKG_CONFIG_EXTRA_LIBS_PRIVATE "${AVIF_PKG_CONFIG_EXTRA_LIBS_PRIVATE} -lyuv")
endif(AVIF_LIBYUV_ENABLED)

set_local_or_system_option("LIBSHARPYUV" "OFF" "Use libsharpyuv.")
check_avif_option(AVIF_LIBSHARPYUV TARGET sharpyuv::sharpyuv PKG_NAME libsharpyuv)
if(AVIF_LIBSHARPYUV_ENABLED)
    message(STATUS "libavif: libsharpyuv found; sharp rgb to yuv conversion enabled.")
    set(AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE "${AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE} libsharpyuv")
    target_compile_definitions(avif_obj PRIVATE -DAVIF_LIBSHARPYUV_ENABLED=1)
    avif_target_link_library(sharpyuv::sharpyuv)
endif(AVIF_LIBSHARPYUV_ENABLED)

option(AVIF_LOCAL_LIBXML2 "Build libxml2 by providing your own copy inside the ext subdir. \
libxml2 is used when AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP is ON" OFF
)
if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    if(NOT TARGET LibXml2::LibXml2)
        if(AVIF_LOCAL_LIBXML2)
            include(LocalLibXml2)
        else()
            find_package(LibXml2 QUIET)
        endif()
    endif()
endif()
# ---------------------------------------------------------------------------------------

# Enable all warnings
include(CheckCCompilerFlag)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(STATUS "libavif: Enabling warnings for Clang")
    target_compile_options(avif_obj PUBLIC $<BUILD_INTERFACE:-Wall -Wextra -Wshorten-64-to-32>)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    message(STATUS "libavif: Enabling warnings for GCC")
    target_compile_options(avif_obj PUBLIC $<BUILD_INTERFACE:-Wall -Wextra>)
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    message(STATUS "libavif: Enabling warnings for MSVC")
    target_compile_options(
        avif_obj
        PUBLIC $<BUILD_INTERFACE:
               /W4
               /wd4324 # Disable: structure was padded due to alignment specifier
               /wd4996 # Disable: potentially unsafe stdlib methods
               # This tells MSVC to read source code as UTF-8 and assume console can only use ASCII (minimal safe).
               # libavif uses ANSI API to print to console, which is not portable between systems using different
               # languages and results in mojibake unless we only use codes shared by every code page: ASCII.
               # A C4556 warning will be generated on violation.
               # Commonly used /utf-8 flag assumes UTF-8 for both source and console, which is usually not the case.
               # Warnings can be suppressed but there will still be random characters printed to the console.
               /source-charset:utf-8
               /execution-charset:us-ascii
               >
    )
else()
    message(FATAL_ERROR "libavif: Unknown compiler, bailing out")
endif()

if(AVIF_ENABLE_WERROR)
    # Warnings as errors
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(avif_obj PUBLIC $<BUILD_INTERFACE:-Werror>)
    elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
        target_compile_options(avif_obj PUBLIC $<BUILD_INTERFACE:/WX>)
    else()
        message(FATAL_ERROR "libavif: Unknown compiler, bailing out")
    endif()
endif()

if(AVIF_ENABLE_WUNUSED_RESULT)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21.0)
        set(CMAKE_C_STANDARD 23)
        set_property(TARGET avif_obj PROPERTY C_STANDARD 23)
    else()
        unset(CMAKE_C_STANDARD)
        set_property(TARGET avif_obj PROPERTY C_STANDARD)
        target_compile_options(avif_obj PUBLIC $<BUILD_INTERFACE:$<$<COMPILE_LANGUAGE:C>:-std=gnu2x>>)
    endif()
    target_compile_options(avif_obj PUBLIC $<BUILD_INTERFACE:-Wunused-result>)
    target_compile_definitions(avif_obj PUBLIC $<BUILD_INTERFACE:AVIF_ENABLE_NODISCARD=1>)
endif()

if(AVIF_ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
        message(STATUS "libavif: Enabling coverage for Clang")
        target_compile_options(avif_obj PUBLIC $<BUILD_INTERFACE:-fprofile-instr-generate -fcoverage-mapping -O0>)
        target_compile_options(avif PUBLIC $<BUILD_INTERFACE:-fprofile-instr-generate -fcoverage-mapping -O0>)
        set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-fprofile-instr-generate -fcoverage-mapping")
    else()
        # TODO: Add support for other compilers
        message(WARNING "libavif: Ignoring request for coverage (AVIF_ENABLE_COVERAGE); only clang is currently supported.")
        set(AVIF_ENABLE_COVERAGE OFF)
    endif()
endif()

if(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
    add_compile_definitions(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
endif()

if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    add_compile_definitions(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
endif()

if(AVIF_ENABLE_EXPERIMENTAL_AVIR)
    add_compile_definitions(AVIF_ENABLE_EXPERIMENTAL_AVIR)
endif()

set(AVIF_SRCS
    src/alpha.c
    src/avif.c
    src/colr.c
    src/colrconvert.c
    src/diag.c
    src/exif.c
    src/io.c
    src/mem.c
    src/obu.c
    src/rawdata.c
    src/read.c
    src/reformat.c
    src/reformat_libsharpyuv.c
    src/reformat_libyuv.c
    src/scale.c
    src/stream.c
    src/utils.c
    src/write.c
)
if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
    list(APPEND AVIF_SRCS src/gainmap.c)
endif()

if(AVIF_ENABLE_COMPLIANCE_WARDEN)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/ComplianceWarden")
        message(FATAL_ERROR "AVIF_ENABLE_COMPLIANCE_WARDEN: ext/ComplianceWarden is missing, bailing out")
    endif()

    set(AVIF_USE_CXX ON)
    target_compile_definitions(avif_obj PRIVATE AVIF_ENABLE_COMPLIANCE_WARDEN)

    list(
        APPEND
        AVIF_SRCS
        src/compliance.cc
        ext/ComplianceWarden/src/app/cw.cpp
        ext/ComplianceWarden/src/app/options.cpp
        ext/ComplianceWarden/src/app/report_std.cpp
        ext/ComplianceWarden/src/app/report_json.cpp
        ext/ComplianceWarden/src/utils/common_boxes.cpp
        ext/ComplianceWarden/src/utils/tools.cpp
        ext/ComplianceWarden/src/utils/av1_utils.cpp
        ext/ComplianceWarden/src/utils/isobmff_utils.cpp
        ext/ComplianceWarden/src/utils/isobmff_derivations.cpp
        ext/ComplianceWarden/src/utils/spec_utils.cpp
        ext/ComplianceWarden/src/specs/av1_hdr10plus/av1_hdr10plus.cpp
        ext/ComplianceWarden/src/specs/avif/avif.cpp
        ext/ComplianceWarden/src/specs/avif/profiles.cpp
        ext/ComplianceWarden/src/specs/avif/utils.cpp
        ext/ComplianceWarden/src/specs/isobmff/isobmff.cpp
        ext/ComplianceWarden/src/specs/heif/heif.cpp
        ext/ComplianceWarden/src/specs/miaf/miaf.cpp
        ext/ComplianceWarden/src/specs/miaf/audio.cpp
        ext/ComplianceWarden/src/specs/miaf/brands.cpp
        ext/ComplianceWarden/src/specs/miaf/derivations.cpp
        ext/ComplianceWarden/src/specs/miaf/colours.cpp
        ext/ComplianceWarden/src/specs/miaf/num_pixels.cpp
        ext/ComplianceWarden/src/specs/miaf/profiles.cpp
        ext/ComplianceWarden/src/cw_version.cpp
    )
endif()

target_sources(avif_obj PRIVATE ${AVIF_SRCS})

# Only applicable to macOS. In GitHub CI's macos-latest os image, this prevents using the libpng
# and libjpeg headers from /Library/Frameworks/Mono.framework/Headers instead of
# /usr/local/include.
set(CMAKE_FIND_FRAMEWORK LAST)

if(UNIX OR MINGW)
    # Find out if we have threading available
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)
    target_link_libraries(avif_obj PUBLIC m Threads::Threads)
endif()

if(NOT AVIF_LIBYUV_ENABLED)
    target_sources(
        avif_obj
        PRIVATE third_party/libyuv/source/scale.c third_party/libyuv/source/scale_common.c third_party/libyuv/source/scale_any.c
                third_party/libyuv/source/row_common.c third_party/libyuv/source/planar_functions.c
    )
    if(DEFINED ANDROID_ABI OR DEFINED APPLE)
        # When building third_party/libyuv/source/scale.c, some functions use
        # some of the parameters only inside an assert statement. This causes
        # unused parameter warnings when building for Android. Suppress the
        # warning in that case.
        target_compile_options(avif_obj PRIVATE -Wno-unused-parameter)
    endif()
endif()

check_avif_option(AVIF_CODEC_DAV1D TARGET dav1d::dav1d PKG_NAME dav1d)
if(AVIF_CODEC_DAV1D_ENABLED)
    target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_DAV1D=1)
    target_sources(avif_obj PRIVATE src/codec_dav1d.c)

    if(UNIX AND NOT APPLE)
        target_link_libraries(dav1d::dav1d INTERFACE ${CMAKE_DL_LIBS}) # for dlsym
    endif()

    avif_target_link_library(dav1d::dav1d)

    message(STATUS "libavif: Codec enabled: dav1d (decode)")
    set(AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE "${AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE} dav1d")
endif()

check_avif_option(AVIF_CODEC_LIBGAV1 TARGET libgav1::libgav1 PKG_NAME libgav1)
if(AVIF_CODEC_LIBGAV1_ENABLED)
    target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_LIBGAV1=1)
    target_sources(avif_obj PRIVATE src/codec_libgav1.c)
    avif_target_link_library(libgav1::libgav1)

    message(STATUS "libavif: Codec enabled: libgav1 (decode)")
endif()

check_avif_option(AVIF_CODEC_RAV1E TARGET rav1e::rav1e PKG_NAME rav1e)
if(AVIF_CODEC_RAV1E_ENABLED)
    target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_RAV1E=1)
    target_sources(avif_obj PRIVATE src/codec_rav1e.c)

    # Unfortunately, rav1e requires a few more libraries
    if(WIN32)
        target_link_libraries(rav1e::rav1e INTERFACE ntdll.lib userenv.lib ws2_32.lib bcrypt.lib)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(rav1e::rav1e INTERFACE ${CMAKE_DL_LIBS}) # for backtrace
    endif()

    avif_target_link_library(rav1e::rav1e)

    message(STATUS "libavif: Codec enabled: rav1e (encode)")
    set(AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE "${AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE} rav1e")
endif()

check_avif_option(AVIF_CODEC_SVT TARGET SvtAv1Enc PKG_NAME svt)
if(AVIF_CODEC_SVT_ENABLED)
    target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_SVT=1)
    target_sources(avif_obj PRIVATE src/codec_svt.c)
    avif_target_link_library(SvtAv1Enc)

    message(STATUS "libavif: Codec enabled: svt (encode)")
    set(AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE "${AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE} SvtAv1Enc")
endif()

check_avif_option(AVIF_CODEC_AOM TARGET aom PKG_NAME aom)
if(AVIF_CODEC_AOM_ENABLED)
    target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_AOM=1)
    if(AVIF_CODEC_AOM_ENCODE AND AVIF_CODEC_AOM_DECODE)
        set(AVIF_CODEC_AOM_ENCODE_DECODE_CONFIG "encode/decode")
        target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_AOM_ENCODE=1 -DAVIF_CODEC_AOM_DECODE=1)
    elseif(AVIF_CODEC_AOM_ENCODE)
        set(AVIF_CODEC_AOM_ENCODE_DECODE_CONFIG "encode only")
        target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_AOM_ENCODE=1)
    elseif(AVIF_CODEC_AOM_DECODE)
        set(AVIF_CODEC_AOM_ENCODE_DECODE_CONFIG "decode only")
        target_compile_definitions(avif_obj PRIVATE -DAVIF_CODEC_AOM_DECODE=1)
    else()
        message(
            FATAL_ERROR
                "libavif: AVIF_CODEC_AOM is on, but both AVIF_CODEC_AOM_ENCODE and AVIF_CODEC_AOM_DECODE are off. Disable AVIF_CODEC_AOM to disable both parts of the codec."
        )
    endif()
    target_sources(avif_obj PRIVATE src/codec_aom.c)

    avif_target_link_library(aom)

    message(STATUS "libavif: Codec enabled: aom (${AVIF_CODEC_AOM_ENCODE_DECODE_CONFIG})")
    set(AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE "${AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE} aom")
endif()

check_avif_option(AVIF_CODEC_AVM TARGET aom PKG_NAME avm)
if(AVIF_CODEC_AVM_ENABLED)
    message(WARNING "libavif: AV2 support with avm is experimental. Only use for testing.")

    # The avm repository is a fork of aom and inherited a lot of folders, files and build artifacts named the same way.
    # Having both dependencies at the same time generates conflicts in includes, binary lookups etc.
    if(AVIF_CODEC_AOM_ENABLED)
        message(FATAL_ERROR "libavif: aom conflicts with avm, bailing out")
    endif()

    target_compile_definitions(avif_obj PUBLIC -DAVIF_CODEC_AVM=1)
    target_sources(avif_obj PRIVATE src/codec_avm.c)

    avif_target_link_library(aom)

    message(STATUS "libavif: Codec enabled: avm (encode/decode)")
endif()

if(NOT AVIF_CODEC_AOM_ENABLED
   AND NOT AVIF_CODEC_DAV1D_ENABLED
   AND NOT AVIF_CODEC_LIBGAV1_ENABLED
   AND NOT AVIF_CODEC_AVM_ENABLED
)
    message(WARNING "libavif: No decoding library is enabled.")
endif()

set_target_properties(avif_obj PROPERTIES C_VISIBILITY_PRESET hidden)
target_include_directories(avif_obj PUBLIC $<BUILD_INTERFACE:${libavif_SOURCE_DIR}/include>)
if(NOT AVIF_LIBYUV_ENABLED)
    target_include_directories(avif_obj PRIVATE ${libavif_SOURCE_DIR}/third_party/libyuv/include/)
endif()
if(AVIF_ENABLE_COMPLIANCE_WARDEN)
    target_include_directories(avif_obj PRIVATE ${libavif_SOURCE_DIR}/ext/ComplianceWarden/src/utils/)
endif()
set(AVIF_PKG_CONFIG_EXTRA_CFLAGS "")
if(BUILD_SHARED_LIBS)
    target_compile_definitions(avif_obj PRIVATE AVIF_DLL AVIF_BUILDING_SHARED_LIBS)
    set(AVIF_PKG_CONFIG_EXTRA_CFLAGS " -DAVIF_DLL")
endif()

# Main avif library.
set_target_properties(avif PROPERTIES VERSION ${LIBRARY_VERSION} SOVERSION ${LIBRARY_SOVERSION})
target_link_libraries(avif PRIVATE avif_obj)
target_include_directories(avif PUBLIC $<BUILD_INTERFACE:${libavif_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(avif INTERFACE AVIF_DLL)
    if(AVIF_USE_CXX)
        set_target_properties(avif PROPERTIES LINKER_LANGUAGE "CXX")
    endif()
endif()

# Give access to functions defined in internal.h when BUILD_SHARED_LIBS is ON, to tests for example.
# The avif_internal target should not be used by external code.
if(BUILD_SHARED_LIBS)
    add_library(avif_internal STATIC)
    target_link_libraries(avif_internal PRIVATE avif_obj)
    target_include_directories(avif_internal PUBLIC ${libavif_SOURCE_DIR}/include)
    # Define the following to avoid linking against avif and avif_internal at the same time.
    target_compile_definitions(avif_internal PUBLIC AVIF_USING_STATIC_LIBS)
else()
    include(merge_static_libs)
    set_target_properties(avif PROPERTIES AVIF_LOCAL ON)
    merge_static_libs(avif_static avif)
    # Set the avif target's output to "avif_internal" and set the output name of
    # the combined static archive target (avif_static)) to avif, so that libavif.a
    # is the merged archive.
    set_target_properties(avif_static PROPERTIES OUTPUT_NAME avif EXPORT_NAME avif)
    set_target_properties(avif PROPERTIES OUTPUT_NAME avif_internal EXPORT_NAME avif_internal)
    add_library(avif_internal ALIAS avif)
endif()

option(AVIF_BUILD_EXAMPLES "Build avif examples." OFF)
if(AVIF_BUILD_EXAMPLES)
    set(AVIF_EXAMPLES avif_example_decode_memory avif_example_decode_file avif_example_decode_streaming avif_example_encode)

    foreach(EXAMPLE ${AVIF_EXAMPLES})
        add_executable(${EXAMPLE} examples/${EXAMPLE}.c)
        if(AVIF_USE_CXX)
            set_target_properties(${EXAMPLE} PROPERTIES LINKER_LANGUAGE "CXX")
        endif()
        target_link_libraries(${EXAMPLE} avif)
    endforeach()
endif()

if(CMAKE_SKIP_INSTALL_RULES)
    set(SKIP_INSTALL_ALL TRUE)
endif()

if(NOT SKIP_INSTALL_ALL)
    include(GNUInstallDirs)
endif()

if(AVIF_CODEC_AOM_ENABLED)
    get_target_property(AOM_INTERFACE_LINK_LIBRARIES aom INTERFACE_LINK_LIBRARIES)
    if(AOM_INTERFACE_LINK_LIBRARIES MATCHES vmaf)
        # vmaf only affects the avifenc and avifdec targets below, not the avif library nor examples targets.
        set(AVIF_USE_CXX ON)
    endif()
endif()

if(AVIF_USE_CXX)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 14)
endif()

option(AVIF_BUILD_APPS "Build avif apps." OFF)
option(AVIF_BUILD_TESTS "Build avif tests." OFF)
option(
    AVIF_ENABLE_GOLDEN_TESTS
    "Build tests that compare encoding outputs to golden files. Needs AVIF_BUILD_APPS=ON, and depends on MP4box which can be built with ext/mp4box.sh"
    OFF
)
option(AVIF_ENABLE_GTEST
       "Build avif C++ tests, which depend on GoogleTest. Requires GoogleTest. Has no effect unless AVIF_BUILD_TESTS is ON." ON
)
option(
    AVIF_ENABLE_FUZZTEST
    "Build avif fuzztest targets. Requires Google FuzzTest. Has no effect unless AVIF_BUILD_TESTS and AVIF_ENABLE_GTEST are ON."
    OFF
)

if(AVIF_BUILD_APPS OR (AVIF_BUILD_TESTS AND (AVIF_ENABLE_FUZZTEST OR AVIF_ENABLE_GTEST)))
    if(AVIF_ZLIBPNG STREQUAL "OFF")
        message(FATAL_ERROR "libavif: AVIF_ZLIBPNG cannot be OFF when AVIF_BUILD_APPS or AVIF_BUILD_TESTS is ON")
    elseif(AVIF_ZLIBPNG STREQUAL "SYSTEM")
        find_package(ZLIB REQUIRED)
        find_package(PNG 1.6.32 REQUIRED) # 1.6.32 or above for png_get_eXIf_1()/png_set_eXIf_1() and iTXt (for XMP).
    endif()
    if(AVIF_JPEG STREQUAL "OFF")
        message(FATAL_ERROR "libavif: AVIF_JPEG cannot be OFF when AVIF_BUILD_APPS or AVIF_BUILD_TESTS is ON")
    elseif(AVIF_JPEG STREQUAL "SYSTEM")
        find_package(JPEG REQUIRED)
    endif()

    add_library(
        avif_apps_obj OBJECT $<TARGET_OBJECTS:avif_obj> apps/shared/avifexif.c apps/shared/avifjpeg.c apps/shared/avifpng.c
                             apps/shared/avifutil.c apps/shared/iccmaker.c apps/shared/y4m.c third_party/iccjpeg/iccjpeg.c
    )
    # Instead of building avif_apps/avif_apps_internal and linking to avif/avif_internal, avif_apps_obj only does one compilation.
    # Still, the compile definitions needs to be passed from avif to avif_apps. The following also passes them to avif_apps_internal but AVIF_DLL does not impact avif_apps_internal.
    target_compile_definitions(avif_apps_obj PRIVATE $<BUILD_INTERFACE:$<TARGET_PROPERTY:avif,INTERFACE_COMPILE_DEFINITIONS>>)
    target_link_libraries(avif_apps_obj PUBLIC avif_obj PNG::PNG ZLIB::ZLIB JPEG::JPEG)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(avif_apps_obj PRIVATE m)
    endif()
    # In GitHub CI's macos-latest os image, /usr/local/include has not only the headers of libpng
    # and libjpeg but also the headers of an older version of libavif. Put the avif include
    # directory before ${PNG_PNG_INCLUDE_DIR} ${JPEG_INCLUDE_DIR} to prevent picking up old libavif
    # headers from /usr/local/include.
    target_include_directories(avif_apps_obj PRIVATE third_party/iccjpeg)
    target_include_directories(avif_apps_obj SYSTEM PRIVATE ${PNG_PNG_INCLUDE_DIR} ${JPEG_INCLUDE_DIR})

    if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
        if(TARGET LibXml2::LibXml2)
            set(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION TRUE)
            add_compile_definitions(AVIF_ENABLE_EXPERIMENTAL_JPEG_GAIN_MAP_CONVERSION)
            target_link_libraries(avif_apps_obj PRIVATE LibXml2::LibXml2)
        else()
            message(STATUS "libavif: libxml2 not found; avifenc will ignore any gain map in jpeg files")
        endif()
    endif()

    # Main avif_apps library.
    add_library(avif_apps STATIC)
    target_link_libraries(avif_apps PUBLIC avif PRIVATE avif_apps_obj)
    target_include_directories(avif_apps INTERFACE apps/shared)

    # avif_apps_internal is to use when linking to avif_internal.
    if(BUILD_SHARED_LIBS)
        add_library(avif_apps_internal STATIC)
        target_link_libraries(avif_apps_internal PUBLIC avif_internal PRIVATE avif_apps_obj)
        target_include_directories(avif_apps_internal INTERFACE apps/shared)
    else()
        add_library(avif_apps_internal ALIAS avif_apps)
    endif()
endif()

if(AVIF_BUILD_APPS)
    add_executable(avifenc apps/avifenc.c)
    if(WIN32)
        # MinGW doesn't have a manifest tool (mt.exe), so we need to wrap
        # utf8.manifest in a resource-definition script (.rc file).
        target_sources(avifenc PRIVATE apps/utf8.rc)
        # The MSVC linker (link.exe) fails with the following error if we
        # don't use the /MANIFEST:NO linker option:
        #   /MANIFEST:EMBED,ID=1" failed (exit code 1123) with the following
        #   output:
        #   CVTRES : fatal error CVT1100: duplicate resource.  type:MANIFEST,
        #   name:1, language:0x0409
        if(NOT MINGW)
            target_link_options(avifenc PRIVATE /MANIFEST:NO)
        endif()
    endif()
    if(AVIF_USE_CXX)
        set_target_properties(avifenc PROPERTIES LINKER_LANGUAGE "CXX")
    endif()
    target_link_libraries(avifenc avif_apps)
    add_executable(avifdec apps/avifdec.c)
    if(WIN32)
        target_sources(avifdec PRIVATE apps/utf8.rc)
        if(NOT MINGW)
            target_link_options(avifdec PRIVATE /MANIFEST:NO)
        endif()
    endif()
    if(AVIF_USE_CXX)
        set_target_properties(avifdec PROPERTIES LINKER_LANGUAGE "CXX")
    endif()
    target_link_libraries(avifdec avif_apps)

    if(NOT SKIP_INSTALL_APPS AND NOT SKIP_INSTALL_ALL)
        install(
            TARGETS avifenc avifdec
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        )
    endif()
    if(AVIF_ENABLE_EXPERIMENTAL_GAIN_MAP)
        include(LocalLibargparse)
        if(TARGET libargparse)
            set(AVIF_ENABLE_AVIFGAINMAPUTIL TRUE)

            set(AVIFGAINMAPUTIL_SRCS
                apps/avifgainmaputil/avifgainmaputil.cc
                apps/avifgainmaputil/convert_command.cc
                apps/avifgainmaputil/combine_command.cc
                apps/avifgainmaputil/extractgainmap_command.cc
                apps/avifgainmaputil/imageio.cc
                apps/avifgainmaputil/printmetadata_command.cc
                apps/avifgainmaputil/tonemap_command.cc
                apps/avifgainmaputil/program_command.cc
                apps/avifgainmaputil/swapbase_command.cc
            )

            add_executable(avifgainmaputil "${AVIFGAINMAPUTIL_SRCS}")
            if(WIN32)
                target_sources(avifgainmaputil PRIVATE apps/utf8.rc)
                if(NOT MINGW)
                    target_link_options(avifgainmaputil PRIVATE /MANIFEST:NO)
                endif()
            endif()
            set_target_properties(avifgainmaputil PROPERTIES LINKER_LANGUAGE "CXX")
            target_include_directories(avifgainmaputil PRIVATE apps/avifgainmaputil/)
            target_link_libraries(avifgainmaputil libargparse avif_apps)
            # Don't add avifgainmaputil to installed apps for now.
        endif()
    endif()
endif()

if(AVIF_BUILD_TESTS)
    enable_testing() # Allow ctest to be called from top-level directory.
    add_subdirectory(tests)
endif()

option(AVIF_BUILD_MAN_PAGES "Build avif man pages." OFF)
if(AVIF_BUILD_MAN_PAGES)
    if(AVIF_BUILD_APPS)
        find_program(PANDOC_EXE pandoc)
        if(PANDOC_EXE)
            message(STATUS "libavif: Using pandoc: ${PANDOC_EXE}")
        else()
            message(FATAL_ERROR "libavif: Pandoc is missing, bailing out")
        endif()

        set(MAN_PAGES avifenc.1 avifdec.1)

        foreach(MAN_PAGE ${MAN_PAGES})
            add_custom_command(
                OUTPUT ${MAN_PAGE}
                COMMAND ${PANDOC_EXE} -s -V "footer=libavif ${PROJECT_VERSION}" -f markdown -t man -o
                        "${CMAKE_CURRENT_BINARY_DIR}/${MAN_PAGE}" "${CMAKE_CURRENT_SOURCE_DIR}/doc/${MAN_PAGE}.md"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/doc/${MAN_PAGE}.md"
                VERBATIM
            )
        endforeach()
        add_custom_target(man_pages ALL DEPENDS ${MAN_PAGES})

        foreach(MAN_PAGE ${MAN_PAGES})
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${MAN_PAGE}" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
        endforeach()
    else()
        message(WARNING "libavif: No man pages are built (AVIF_BUILD_MAN_PAGES); AVIF_BUILD_APPS must be on.")
    endif()
endif()

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    if(BUILD_SHARED_LIBS)
        set(LIBAVIF_INSTALL_TARGET avif)
    else()
        set(LIBAVIF_INSTALL_TARGET avif_static)
    endif()
    install(
        TARGETS ${LIBAVIF_INSTALL_TARGET}
        EXPORT ${PROJECT_NAME}-config
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )

    # Enable CMake configs in VCPKG mode
    if(BUILD_SHARED_LIBS OR VCPKG_TARGET_TRIPLET)
        install(EXPORT ${PROJECT_NAME}-config DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

        include(CMakePackageConfigHelpers)
        write_basic_package_version_file(
            ${PROJECT_NAME}-config-version.cmake VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
                DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
        )
    endif()

    # Handle both relative and absolute paths (e.g. NixOS) for a relocatable package
    if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
        set(PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    else()
        set(PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()
    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    else()
        set(PC_LIBDIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
    endif()
    configure_file(libavif.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libavif.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libavif.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
    install(FILES include/avif/avif.h include/avif/avif_cxx.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/avif")
endif()

# ---------------------------------------------------------------------------------------
# Win32 (Visual Studio) fixups

macro(avif_set_folder_safe target folder)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER ${folder})
    endif()
endmacro()

macro(avif_exclude_safe target)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD True)
    endif()
endmacro()

if(WIN32)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    avif_set_folder_safe(avif "ext/avif")
    if(AVIF_BUILD_EXAMPLES)
        foreach(EXAMPLE ${AVIF_EXAMPLES})
            avif_set_folder_safe(${EXAMPLE} "ext/avif/examples")
        endforeach()
    endif()
    if(AVIF_ZLIBPNG STREQUAL "LOCAL")
        avif_set_folder_safe(example "ext/zlibpng")
        avif_set_folder_safe(genfiles "ext/zlibpng")
        avif_set_folder_safe(minigzip "ext/zlibpng")
        avif_set_folder_safe(png_static "ext/zlibpng")
        avif_set_folder_safe(zlib "ext/zlibpng")
        avif_set_folder_safe(zlibstatic "ext/zlibpng")

        # Don't bother building these targets
        avif_exclude_safe(example)
        avif_exclude_safe(genfiles)
        avif_exclude_safe(minigzip)
    endif()
    if(AVIF_LOCAL_JPEG)
        avif_set_folder_safe(jpeg "ext/libjpeg")
    endif()
    if(AVIF_LOCAL_LIBXML2)
        avif_set_folder_safe(xml2 "ext/libxml2")
    endif()
endif()

add_subdirectory(contrib)
