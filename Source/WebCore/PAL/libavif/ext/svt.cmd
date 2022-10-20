: # If you want to use a local build of SVT-AV1, you must clone the SVT-AV1 repo in this directory first,
: # then enable CMake's AVIF_CODEC_SVT and AVIF_LOCAL_SVT options.
: # cmake and ninja must be in your PATH.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # Switch to a sh-like command if not running in windows
: ; $SHELL svt.sh
: ; exit $?

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

git clone -b v1.2.1 --depth 1 https://gitlab.com/AOMediaCodec/SVT-AV1.git

cd SVT-AV1
cd Build/windows

call build.bat release static no-dec no-apps
cd ../..
mkdir include\svt-av1
copy Source\API\*.h include\svt-av1

cd ..
