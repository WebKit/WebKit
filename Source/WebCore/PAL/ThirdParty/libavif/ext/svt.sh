# If you want to use a local build of SVT-AV1, you must clone the SVT-AV1 repo in this directory first,
# then set CMake's AVIF_CODEC_SVT to LOCAL.
# cmake and ninja must be in your PATH.

set -e

git clone -b v1.7.0 --depth 1 https://gitlab.com/AOMediaCodec/SVT-AV1.git

cd SVT-AV1
cd Build/linux

./build.sh disable-native release static no-dec no-apps
cd ../..
mkdir -p include/svt-av1
cp Source/API/*.h include/svt-av1

cd ..
