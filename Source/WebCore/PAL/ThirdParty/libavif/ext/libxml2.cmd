: # If you want to use a local build of libxml2, you must clone the libxml2 repo in this directory first, then enable CMake's AVIF_LOCAL_LIBXML2 option.
: # The git tag below is known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # libxml2 is released under the MIT License.

git clone -b v2.11.5 --depth 1 https://gitlab.gnome.org/GNOME/libxml2.git

mkdir -p libxml2/build.libavif
cmake libxml2 -B libxml2/build.libavif/ -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=libxml2/install.libavif \
    -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_ZLIB=OFF -DLIBXML2_WITH_LZMA=OFF
ninja -C libxml2/build.libavif
ninja -C libxml2/build.libavif install
