: # If you want to use a local build of libyuv, you must clone the libyuv repo in this directory first, then set CMake's AVIF_LIBYUV to LOCAL.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
: # We recommend building libyuv with clang-cl on Windows, because most of libyuv's assembly code is
: # written in GCC inline assembly syntax, which MSVC doesn't support. Run this if you have clang-cl
: # installed:
: #     set "CC=clang-cl" && set "CXX=clang-cl"

git clone --single-branch https://chromium.googlesource.com/libyuv/libyuv

cd libyuv
: # When changing the commit below to a newer version of libyuv, it is best to make sure it is being used by chromium,
: # because the test suite of chromium provides additional test coverage of libyuv.
: # It can be looked up at https://source.chromium.org/chromium/chromium/src/+/main:DEPS?q=libyuv.
git checkout 464c51a0

mkdir build
cd build

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON ..
ninja yuv
cd ../..
