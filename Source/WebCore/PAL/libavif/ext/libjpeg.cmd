: # If you want to use a local build of jpeg, you must clone the repos in this directory first,
: # then enable CMake's AVIF_LOCAL_JPEG.
: # This git tag isn't likely to move much, as libjpeg isn't actively developed anymore.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

git clone --depth 1 https://github.com/joedrago/libjpeg.git
