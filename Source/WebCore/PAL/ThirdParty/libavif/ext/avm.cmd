: # If you want to use a local build of libavm, you must clone the avm repo in this directory first, then set CMake's AVIF_CODEC_AVM to LOCAL.
: # The git SHA below is known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cmake and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

git clone -b research-v6.0.0 --depth 1 https://gitlab.com/AOMediaCodec/avm.git

cd avm

: # The following fix avoids errors such as "libaom.a: in function `write_frame_hash': bitstream.c: undefined reference to `MD5Init'"
: # TODO(yguyon): Remove at next version bump. It was fixed in https://gitlab.com/AOMediaCodec/avm/-/merge_requests/752.
sed 's-"${AOM_ROOT}/av1/encoder/dwt.h"-"${AOM_ROOT}/av1/encoder/dwt.h"\n  "${AOM_ROOT}/common/md5_utils.c"\n  "${AOM_ROOT}/common/md5_utils.h"\n  "${AOM_ROOT}/common/rawenc.c"\n  "${AOM_ROOT}/common/rawenc.h"-g' av1/av1.cmake > av1/av1.cmake.sed
mv av1/av1.cmake.sed av1/av1.cmake

mkdir build.libavif
cd build.libavif

cmake -G Ninja -DBUILD_SHARED_LIBS=OFF -DCONFIG_PIC=1 -DCMAKE_BUILD_TYPE=Release -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0 ..
ninja
cd ../..
