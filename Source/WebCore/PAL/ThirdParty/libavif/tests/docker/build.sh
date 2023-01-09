#!/bin/bash

# Tests system-wide libavif shared library installation correct behavior, using Ubuntu in Docker. Run:
#
#     docker run -it ubuntu
#
# ... then run this script inside of there. When it finishes, avifenc and avifdec should
# be in /usr/bin and offer all codecs chosen in the last cmake command in this script.
#
# An easy way to get this script into your running Docker container is to type (in the container):
#
#     cat > docker_ubuntu_shared_libs.sh
#
# Paste the contents of this script into the terminal, then hit Ctrl+D. You can then just run:
#
#     bash docker_ubuntu_shared_libs.sh

set -e

# build env
apt update
DEBIAN_FRONTEND="noninteractive" apt install -y build-essential libjpeg-dev libpng-dev libssl-dev ninja-build cmake pkg-config git perl vim curl python3-pip
pip3 install meson

# Rust env
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source $HOME/.cargo/env
cargo install cargo-c

# NASM
cd
curl -L https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz | tar xvz
cd nasm-2.15.05
./configure --prefix=/usr && make -j2 && make install
nasm --version

# aom
cd
git clone -b v3.5.0 --depth 1 https://aomedia.googlesource.com/aom
cd aom
mkdir build.avif
cd build.avif
cmake -G Ninja -DBUILD_SHARED_LIBS=1 -DCMAKE_INSTALL_PREFIX=/usr ..
ninja install

# dav1d
cd
git clone -b 1.0.0 --depth 1 https://code.videolan.org/videolan/dav1d.git
cd dav1d
mkdir build
cd build
meson --prefix=/usr --buildtype release ..
ninja install

# libgav1
cd
git clone -b v0.18.0 --depth 1 https://chromium.googlesource.com/codecs/libgav1
cd libgav1
mkdir build
cd build
cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=1 -DCMAKE_BUILD_TYPE=Release -DLIBGAV1_THREADPOOL_USE_STD_MUTEX=1 -DLIBGAV1_ENABLE_EXAMPLES=0 -DLIBGAV1_ENABLE_TESTS=0 ..
ninja install

# rav1e
cd
git clone -b v0.5.1 --depth 1 https://github.com/xiph/rav1e.git
cd rav1e
cargo cinstall --prefix=/usr --release

# SVT-AV1
cd
git clone -b v1.2.1 --depth 1 https://gitlab.com/AOMediaCodec/SVT-AV1.git
cd SVT-AV1
cd Build
cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ..
ninja install

# libavif
cd
git clone --depth 1 https://github.com/AOMediaCodec/libavif.git
cd libavif
mkdir build
cd build
cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DAVIF_CODEC_AOM=1 -DAVIF_CODEC_DAV1D=1 -DAVIF_CODEC_LIBGAV1=1 -DAVIF_CODEC_RAV1E=1 -DAVIF_CODEC_SVT=1 -DAVIF_BUILD_APPS=1 ..
ninja install

# If we made it here, show off the goods!
echo " * libavif contents in /usr:"
echo
find /usr | grep avif
echo

echo " * avifenc location:" `which avifenc`
echo
avifenc -h

echo " * avifdec location:" `which avifdec`
echo
avifdec -h
