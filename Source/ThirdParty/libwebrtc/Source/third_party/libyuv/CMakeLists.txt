# CMakeLists for libyuv
# Originally created for "roxlu build system" to compile libyuv on windows
# Run with -DUNIT_TEST=ON to build unit tests

cmake_minimum_required( VERSION 3.16 )
project ( YUV C CXX )	# "C" is required even for C++ projects
option( UNIT_TEST "Built unit tests" OFF )

include(CheckCSourceCompiles)

set ( ly_base_dir	${PROJECT_SOURCE_DIR} )
set ( ly_src_dir	${ly_base_dir}/source )
set ( ly_inc_dir	${ly_base_dir}/include )
set ( ly_tst_dir	${ly_base_dir}/unit_test )
set ( ly_lib_name	yuv )
set ( ly_lib_static	${ly_lib_name} )
set ( ly_lib_shared	${ly_lib_name}_shared )

# We cannot use GLOB here since we want to be able to separate out files that
# need particular flags to enable architecture extensions like AArch64's SVE.
# TODO: More of these files could be separated out for different architectures.
set ( ly_common_source_files
  ${ly_src_dir}/compare.cc
  ${ly_src_dir}/compare_common.cc
  ${ly_src_dir}/compare_gcc.cc
  ${ly_src_dir}/compare_win.cc
  ${ly_src_dir}/convert_argb.cc
  ${ly_src_dir}/convert.cc
  ${ly_src_dir}/convert_from_argb.cc
  ${ly_src_dir}/convert_from.cc
  ${ly_src_dir}/convert_jpeg.cc
  ${ly_src_dir}/convert_to_argb.cc
  ${ly_src_dir}/convert_to_i420.cc
  ${ly_src_dir}/cpu_id.cc
  ${ly_src_dir}/mjpeg_decoder.cc
  ${ly_src_dir}/mjpeg_validate.cc
  ${ly_src_dir}/planar_functions.cc
  ${ly_src_dir}/rotate_any.cc
  ${ly_src_dir}/rotate_argb.cc
  ${ly_src_dir}/rotate.cc
  ${ly_src_dir}/rotate_common.cc
  ${ly_src_dir}/rotate_gcc.cc
  ${ly_src_dir}/rotate_lsx.cc
  ${ly_src_dir}/rotate_win.cc
  ${ly_src_dir}/row_any.cc
  ${ly_src_dir}/row_common.cc
  ${ly_src_dir}/row_gcc.cc
  ${ly_src_dir}/row_lasx.cc
  ${ly_src_dir}/row_lsx.cc
  ${ly_src_dir}/row_rvv.cc
  ${ly_src_dir}/row_win.cc
  ${ly_src_dir}/scale_any.cc
  ${ly_src_dir}/scale_argb.cc
  ${ly_src_dir}/scale.cc
  ${ly_src_dir}/scale_common.cc
  ${ly_src_dir}/scale_gcc.cc
  ${ly_src_dir}/scale_lsx.cc
  ${ly_src_dir}/scale_rgb.cc
  ${ly_src_dir}/scale_rvv.cc
  ${ly_src_dir}/scale_uv.cc
  ${ly_src_dir}/scale_win.cc
  ${ly_src_dir}/video_common.cc)

file ( GLOB_RECURSE	ly_unittest_sources ${ly_tst_dir}/*.cc )
list ( SORT			ly_unittest_sources )

include_directories( BEFORE ${ly_inc_dir} )

if(MSVC)
  add_definitions ( -D_CRT_SECURE_NO_WARNINGS )
endif()

# Need to set PIC to allow creating shared libraries from object file libraries.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build the set of objects that do not need to be compiled with flags to enable
# particular architecture features.
add_library( ${ly_lib_name}_common_objects OBJECT ${ly_common_source_files} )
set(ly_lib_parts $<TARGET_OBJECTS:${ly_lib_name}_common_objects>)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSPROC)
set(LOONGARCH64_ALIASES loongarch64)
list(FIND LOONGARCH64_ALIASES "${SYSPROC}" LOONGARCH64MATCH)

if(LOONGARCH64MATCH GREATER "-1")
  set(LOONGARCH64 1)
endif()

if(NOT MSVC)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" arch_lowercase)

  if(arch_lowercase MATCHES "^arm" AND NOT arch_lowercase STREQUAL "arm64")
    # Enable Arm Neon kernels.
    add_definitions(-DLIBYUV_NEON=1)
    add_library(${ly_lib_name}_neon OBJECT
      ${ly_src_dir}/compare_neon.cc
      ${ly_src_dir}/rotate_neon.cc
      ${ly_src_dir}/row_neon.cc
      ${ly_src_dir}/scale_neon.cc)
    target_compile_options(${ly_lib_name}_neon PRIVATE -mfpu=neon)
    list(APPEND ly_lib_parts $<TARGET_OBJECTS:${ly_lib_name}_neon>)
  endif()

  if(arch_lowercase STREQUAL "aarch64" OR arch_lowercase STREQUAL "arm64")
    # Enable AArch64 Neon dot-product and i8mm kernels.
    add_library(${ly_lib_name}_neon64 OBJECT
      ${ly_src_dir}/compare_neon64.cc
      ${ly_src_dir}/rotate_neon64.cc
      ${ly_src_dir}/row_neon64.cc
      ${ly_src_dir}/scale_neon64.cc)
    target_compile_options(${ly_lib_name}_neon64 PRIVATE -march=armv8.2-a+dotprod+i8mm)
    list(APPEND ly_lib_parts $<TARGET_OBJECTS:${ly_lib_name}_neon64>)

    # Enable AArch64 SVE kernels.
    add_library(${ly_lib_name}_sve OBJECT
      ${ly_src_dir}/row_sve.cc)
    target_compile_options(${ly_lib_name}_sve PRIVATE -march=armv8.5-a+i8mm+sve2)
    list(APPEND ly_lib_parts $<TARGET_OBJECTS:${ly_lib_name}_sve>)

    set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    set(OLD_CMAKE_TRY_COMPILE_TARGET_TYPE ${CMAKE_TRY_COMPILE_TARGET_TYPE})
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -march=armv9-a+i8mm+sme")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    # Check whether the compiler can compile SME functions; this fails
    # with Clang for Windows.
    check_c_source_compiles("
__arm_locally_streaming void func(void) { }
int main(void) { return 0; }
    " CAN_COMPILE_SME)
    set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
    set(CMAKE_TRY_COMPILE_TARGET_TYPE ${OLD_CMAKE_TRY_COMPILE_TARGET_TYPE})

    if (CAN_COMPILE_SME)
      # Enable AArch64 SME kernels.
      add_library(${ly_lib_name}_sme OBJECT
        ${ly_src_dir}/rotate_sme.cc
        ${ly_src_dir}/row_sme.cc
        ${ly_src_dir}/scale_sme.cc)
      target_compile_options(${ly_lib_name}_sme PRIVATE -march=armv9-a+i8mm+sme)
      list(APPEND ly_lib_parts $<TARGET_OBJECTS:${ly_lib_name}_sme>)
    else()
      add_definitions(-DLIBYUV_DISABLE_SME)
    endif()
  endif()
endif()

if(LOONGARCH64)
  include(CheckCXXSourceCompiles)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-narrowing")
  check_cxx_source_compiles("
    int main(int argc, char **argv) {
      __asm__ volatile (
         \"vadd.w $vr0, $vr1, $vr1\"
      );
    return 0; }" SUPPORTS_LSX)

  check_cxx_source_compiles("
    int main(int argc, char **argv) {
      __asm__ volatile (
         \"xvadd.w $xr0, $xr1, $xr1\"
      );
    return 0; }" SUPPORTS_LASX)

  if(SUPPORTS_LSX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mlsx")
  endif()
  if(SUPPORTS_LASX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mlasx")
  endif()
endif()

# this creates the static library (.a)
add_library( ${ly_lib_static} STATIC ${ly_lib_parts})

# this creates the shared library (.so)
add_library( ${ly_lib_shared} SHARED ${ly_lib_parts})
set_target_properties( ${ly_lib_shared} PROPERTIES OUTPUT_NAME "${ly_lib_name}" )
set_target_properties( ${ly_lib_shared} PROPERTIES PREFIX "lib" )
if(WIN32)
  set_target_properties( ${ly_lib_shared} PROPERTIES IMPORT_PREFIX "lib" )
endif()

# this creates the cpuid tool
add_executable      ( cpuid ${ly_base_dir}/util/cpuid.c )
target_link_libraries  ( cpuid ${ly_lib_static} )

# this creates the conversion tool
add_executable			( yuvconvert ${ly_base_dir}/util/yuvconvert.cc )
target_link_libraries	( yuvconvert ${ly_lib_static} )

# this creates the yuvconstants tool
add_executable      ( yuvconstants ${ly_base_dir}/util/yuvconstants.c )
target_link_libraries  ( yuvconstants ${ly_lib_static} )

find_package ( JPEG )
if (JPEG_FOUND)
  include_directories( ${JPEG_INCLUDE_DIR} )
  target_link_libraries( ${ly_lib_shared} ${JPEG_LIBRARY} )
  add_definitions( -DHAVE_JPEG )
endif()

if(UNIT_TEST)
  find_library(GTEST_LIBRARY gtest)
  if(GTEST_LIBRARY STREQUAL "GTEST_LIBRARY-NOTFOUND")
    set(GTEST_SRC_DIR_DEFAULT /usr/src/gtest)
    if (CMAKE_CROSSCOMPILING)
      set(GTEST_SRC_DIR_DEFAULT ${CMAKE_SOURCE_DIR}/third_party/googletest/src/googletest)
    endif()
    set(GTEST_SRC_DIR ${GTEST_SRC_DIR_DEFAULT} CACHE STRING "Location of gtest sources")
    if(EXISTS ${GTEST_SRC_DIR}/src/gtest-all.cc)
      message(STATUS "building gtest from sources in ${GTEST_SRC_DIR}")
      set(gtest_sources ${GTEST_SRC_DIR}/src/gtest-all.cc)
      add_library(gtest STATIC ${gtest_sources})
      include_directories(${GTEST_SRC_DIR})
      include_directories(${GTEST_SRC_DIR}/include)
      set(GTEST_LIBRARY gtest)
    else()
      message(FATAL_ERROR "UNIT_TEST is set but unable to find gtest library")
    endif()
  endif()

  add_executable(libyuv_unittest ${ly_unittest_sources})
  target_link_libraries(libyuv_unittest ${ly_lib_name} ${GTEST_LIBRARY})
  find_library(PTHREAD_LIBRARY pthread)
  if(NOT PTHREAD_LIBRARY STREQUAL "PTHREAD_LIBRARY-NOTFOUND")
    target_link_libraries(libyuv_unittest pthread)
  endif()
  if (JPEG_FOUND)
    target_link_libraries(libyuv_unittest ${JPEG_LIBRARY})
  endif()

  if(NACL AND NACL_LIBC STREQUAL "newlib")
    target_link_libraries(libyuv_unittest glibc-compat)
  endif()

  find_library(GFLAGS_LIBRARY gflags)
  if(NOT GFLAGS_LIBRARY STREQUAL "GFLAGS_LIBRARY-NOTFOUND")
    target_link_libraries(libyuv_unittest gflags)
    add_definitions(-DLIBYUV_USE_GFLAGS)
  endif()
endif()


# install the conversion tool, .so, .a, and all the header files
install ( TARGETS yuvconvert DESTINATION bin )
install ( TARGETS ${ly_lib_static}						DESTINATION lib )
install ( TARGETS ${ly_lib_shared} LIBRARY DESTINATION lib RUNTIME DESTINATION bin ARCHIVE DESTINATION lib )
install ( DIRECTORY ${PROJECT_SOURCE_DIR}/include/		DESTINATION include )

# create the .deb and .rpm packages using cpack
include ( CM_linux_packages.cmake )

