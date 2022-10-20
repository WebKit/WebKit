: # If you want to use a local build of libsharpyuv, you must clone the libwebp repo in this directory first,
: # then enable CMake's AVIF_LOCAL_LIBSHARPYUV option.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

: # libsharpyuv is part of the libwebp repo.
git clone --single-branch https://chromium.googlesource.com/webm/libwebp

cd libwebp
git checkout 15a91ab

mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release ..
ninja sharpyuv
cd ../..
