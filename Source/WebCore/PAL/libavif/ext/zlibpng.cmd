: # If you want to use a local build of zlib/libpng, you must clone the repos in this directory first,
: # then enable CMake's AVIF_LOCAL_ZLIBPNG.
: # The git tags below are known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

git clone -b v1.2.12 --depth 1 https://github.com/madler/zlib.git
git clone -b v1.6.37 --depth 1 https://github.com/glennrp/libpng.git
