# Copyright 2022 Google LLC. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.13)

project(avif_android_jni)

# Perform a static build of libavif and link it with the jni shared library so
# that we end up with a single libavif_android.so file in the end.
#
# By default, we build libavif with libgav1 as the AV1 decoder. To change that
# behavior, set up the android builds for the desired decoder and change the
# variables below accordingly.

# To override the variables in the libavif subproject:
# https://stackoverflow.com/questions/3766740/overriding-a-default-option-value-in-cmake-from-a-parent-cmakelists-txt
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(AVIF_LOCAL_LIBGAV1 ON CACHE BOOL "" FORCE)
set(AVIF_CODEC_LIBGAV1 ON CACHE BOOL "" FORCE)

# The current CMake file lives in:
# $LIBAVIF_ROOT/android_jni/avifandroidjni/src/main/jni.
# In order to build libavif, we need to go up 5 directories. If a different
# libavif checkout is to be used, this path has to be updated accordingly.
add_subdirectory(../../../../.. build)

add_library("avif_android" SHARED "libavif_jni.cc")

# Import the cpu-features module to compute the number of threads used for
# decoding.
include(AndroidNdkModules)
android_ndk_import_module_cpufeatures()

target_link_libraries(avif_android jnigraphics avif log cpufeatures)
