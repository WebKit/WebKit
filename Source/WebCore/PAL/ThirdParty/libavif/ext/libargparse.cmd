: # Script to get a local build of libargparse (not to be confused with other similarly named libraries)
: # needed by some of the command line tools in the apps/ directory e.g. avifgainmaputil.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

git clone --single-branch https://github.com/kmurray/libargparse.git

cd libargparse
git checkout ee74d1b53bd680748af14e737378de57e2a0a954

mkdir build
cd build
cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release  ..
ninja
cd ../..
