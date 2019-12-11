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

PREVIOUS_COMMIT_HASH=`grep -m 1 -o -E "[a-z0-9]{40}" ANGLE.plist`

# First, preserve WebKit's additional files
TEMPDIR=`mktemp -d`
function cleanup {
	echo "Copying WebKit-added files into the updated ANGLE:"
	pushd "$TEMPDIR" &> /dev/null
	popd &> /dev/null
    cp -a "$TEMPDIR/." "$ANGLE_DIR"
    rm -rf "$TEMPDIR"
	git ls-files --others --exclude-standard | sed 's/^/     /'
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
git clone https://chromium.googlesource.com/angle/angle .
echo "Successfully downloaded latest ANGLE."
echo -n "Commit hash: "
COMMIT_HASH=`git rev-parse HEAD`
echo "$COMMIT_HASH"
echo ""
echo "Summary of added and removed files since last update:"
echo "________________________________________________________________________"
echo ""
git diff --summary "$PREVIOUS_COMMIT_HASH"
echo "________________________________________________________________________"
echo ""

trap - EXIT
cleanup

echo "Copying src/commit.h to src/id/commit.h"
mkdir -p src/id
cp src/commit.h src/id/
echo "Updating ANGLE.plist commit hashes."
sed -i.bak -e "s/\([^a-z0-9]\)[a-z0-9]\{40\}\([^a-z0-9]\)/\1$COMMIT_HASH\2/g" ANGLE.plist
echo "Updating ANGLE.plist date."
sed -i.bak -e "s/<string>[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]<\/string>/<string>`date +%Y-%m-%d`<\/string>/g" ANGLE.plist
rm ANGLE.plist.bak

echo "Applying patch from changes.diff."
if ! git am changes.diff; then
    echo "ERROR: Patch failed."
    echo "You must resolve merge conflicts manually."
    FAILED_PATCH=true
fi

echo "________________________________________________________________________"
echo ""
echo "There is now a git repository for the latest ANGLE in the directory:"
echo "$ANGLE_DIR"
echo ""
echo "Required manual steps:"
if [ "$FAILED_PATCH" = true ]; then
    echo "    - Resolve the merge conflicts created while applying changes.diff."
fi
echo "    - Update the XCode project with any new or deleted ANGLE source files."
echo "    - Update the CMake files with any new or deleted ANGLE source files."
echo "    - Fix any build/test failures and commit any modified ANGLE files to the"
echo "      ANGLE git repository."
echo "        - Be sure not to commit the WebKit-added files listed above, such as"
echo "          this script or ANGLE.xcodeproj."
echo "    - Once you have committed any changes you made to upstream ANGLE files,"
echo "      regenerate changes.diff using this command:"
echo "          $ git format-patch origin/master --stdout > changes.diff"
echo "    - Once changes.diff is updated, remove the ANGLE git repository by"
echo "      deleting the .git directory:"
echo "          $ rm -rf \"$ANGLE_DIR/.git\""
