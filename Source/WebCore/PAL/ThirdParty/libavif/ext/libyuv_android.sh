#!/bin/bash

# This script will build libyuv for the default ABI targets supported by
# android. You must pass the path to the android NDK as a parameter to this
# script.
#
# Android NDK: https://developer.android.com/ndk/downloads

set -e

if [ $# -ne 1 ]; then
  echo "Usage: ${0} <path_to_android_ndk>"
  exit 1
fi

git clone --single-branch https://chromium.googlesource.com/libyuv/libyuv

cd libyuv
: # When changing the commit below to a newer version of libyuv, it is best to make sure it is being used by chromium,
: # because the test suite of chromium provides additional test coverage of libyuv.
: # It can be looked up at https://source.chromium.org/chromium/chromium/src/+/main:DEPS?q=libyuv.
git checkout 464c51a0

mkdir build
cd build

ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"
for abi in ${ABI_LIST}; do
  mkdir "${abi}"
  cd "${abi}"
  cmake ../.. \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_TOOLCHAIN_FILE=${1}/build/cmake/android.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_ABI=${abi}
  make yuv
  cd ..
done

cd ../..
