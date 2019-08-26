#!/bin/bash

set -e
cd "$(dirname ${BASH_SOURCE[0]})"

ANGLE_DIR="$PWD"

echo "This will clobber any changes you have made in:"
echo "$ANGLE_DIR"
read -p "Are you sure? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Canceled."
    exit
fi

# First, preserve WebKit's additional files
TEMPDIR=`mktemp -d`
function cleanup {
    cp -a "$TEMPDIR/." "$ANGLE_DIR"
    rm -rf "$TEMPDIR"
}
trap cleanup EXIT
mv \
    ANGLE.plist \
    ANGLE.xcodeproj \
    CMakeLists.txt \
    ChangeLog \
    *.cmake \
    Configurations \
    Makefile \
    adjust-angle-include-* \
    update-angle.sh \
    changes.diff \
    "$TEMPDIR"
mkdir "$TEMPDIR/include"
mv include/CMakeLists.txt "$TEMPDIR/include"

# Remove all files including hidden ones, but not . or ..
rm -rf ..?* .[!.]* *
echo "Downloading latest ANGLE via git clone."
git clone https://chromium.googlesource.com/angle/angle . --depth 1
echo "Successfully downloaded latest ANGLE."
echo -n "Commit hash: "
git rev-parse HEAD

trap - EXIT
cleanup

echo "Applying patch from changes.diff."
if ! git am changes.diff; then
    echo "ERROR: Patch failed."
    echo "You must resolve any merge conflicts, then use \`git format-patch master\`"
    echo "to regenerate changes.diff so that it will apply cleanly next time."
    echo "Once you are done, be sure to delete the .git directory."
    FAILED_PATCH=true
else
    rm -rf .git
fi

echo
echo "Required manual steps:"
if [ "$FAILED_PATCH" = true ]; then
    echo "    - Finish the patching process as described above."
fi
echo "    - Update the XCode project with any new or deleted ANGLE source files."
echo "    - Update the CMake files with any new or deleted ANGLE source files."
echo "    - Update ANGLE.plist with the new commit hash."
