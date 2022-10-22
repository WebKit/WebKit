# If you want to use a local build of SVT-AV1, you must clone the SVT-AV1 repo in this directory first,
# then enable CMake's AVIF_CODEC_SVT and AVIF_LOCAL_SVT options.
# cmake and ninja must be in your PATH.

git clone -b v1.2.1 --depth 1 https://gitlab.com/AOMediaCodec/SVT-AV1.git

cd SVT-AV1
cd Build/linux

./build.sh release static no-dec no-apps
cd ../..
mkdir -p include/svt-av1
cp Source/API/*.h include/svt-av1

cd ..
