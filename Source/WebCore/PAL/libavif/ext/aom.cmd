: # If you want to use a local build of libaom, you must clone the aom repo in this directory first, then enable CMake's AVIF_CODEC_AOM and AVIF_LOCAL_AOM options.
: # The git SHA below is known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

git clone -b v3.5.0 --depth 1 https://aomedia.googlesource.com/aom

cd aom
mkdir build.libavif
cd build.libavif

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 ..
ninja
cd ../..
