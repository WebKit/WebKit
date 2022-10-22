: # If you want to use a local build of googletest, you must clone the googletest repo in this directory.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

git clone -b release-1.12.1 --depth 1 https://github.com/google/googletest.git
cd googletest
mkdir build
cd build
: # The gtest_force_shared_crt option makes gtest link the Microsoft C runtime library (CRT) dynamically
: # on Windows:
: # https://github.com/google/googletest/blob/main/googletest/README.md#visual-studio-dynamic-vs-static-runtimes
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_GMOCK=OFF -Dgtest_force_shared_crt=ON ..
cmake --build .
cd ../..
