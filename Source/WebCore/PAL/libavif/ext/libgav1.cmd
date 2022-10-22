: # If you want to use a local build of libgav1, you must clone the libgav1 repo in this directory first, then enable CMake's AVIF_CODEC_LIBGAV1 and AVIF_LOCAL_LIBGAV1 options.
: # The git SHA below is known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

# When updating the libgav1 version, make the same change to libgav1_android.sh.
git clone -b v0.18.0 --depth 1 https://chromium.googlesource.com/codecs/libgav1

cd libgav1
mkdir build
cd build

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIBGAV1_THREADPOOL_USE_STD_MUTEX=1 -DLIBGAV1_ENABLE_EXAMPLES=0 -DLIBGAV1_ENABLE_TESTS=0 ..
ninja
cd ../..
