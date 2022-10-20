: # If you want to use a local build of rav1e, you must clone the rav1e repo in this directory first, then enable CMake's AVIF_CODEC_RAV1E and AVIF_LOCAL_RAV1E options.
: # The git SHA below is known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # cargo must be in your PATH. (use rustup or brew to install)

: # If you're running this on Windows targeting Rust's windows-msvc, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
: #
: # On a successful local build, rav1e/build.libavif/ should contain the necessary headers and static library.

git clone -b v0.5.1 --depth 1 https://github.com/xiph/rav1e.git

cd rav1e
cargo install cargo-c

mkdir build.libavif
cargo cinstall --release --library-type=staticlib --prefix=/usr --destdir build.libavif
cd ..
