#!/bin/bash

set -e
cd "$(dirname ${BASH_SOURCE[0]})"

ANGLE_DIR="$PWD"

function print_rebase_message_and_exit {
    echo
    echo "*** When you are ready, Instead of running git rebase --continue, run"
    echo "*** update-angle.sh again."
    exit 1
}

function cleanup_after_successful_rebase_and_exit {
    cd "$ANGLE_DIR"
    echo
    echo "Regenerating changes.diff"
    git diff origin/master --diff-filter=a > changes.diff
    rm -rf .git
    git add -A .
    echo
    git --no-pager diff --cached Compiler.cmake GLESv2.cmake
    echo
    echo "Success! Rebase complete."
    echo "Above are the changes to Compiler.cmake and GLESv2.cmake."
    echo "Now you'll need to apply the equivalent changes to the ANGLE XCode"
    echo "project. Once that's done you should be ready to upload your patch."
    echo
    exit 0
}

if test -d .git && (test -d "$(git rev-parse --git-path rebase-merge)" || test -d "$(git rev-parse --git-path rebase-apply)"); then
    if ! git rebase --continue; then
        print_rebase_message_and_exit
    fi
    cleanup_after_successful_rebase_and_exit
fi

echo "This script will check out the latest ANGLE and start a git rebase"
echo "to apply WebKit's local ANGLE changes on top of the latest ANGLE master"
echo "branch."
echo
echo "This will clobber any changes you have made in:"
echo "$ANGLE_DIR"
read -p "Are you sure? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Canceled."
    exit
fi

PREVIOUS_COMMIT_HASH=`grep -m 1 -o -E "[a-z0-9]{40}" ANGLE.plist`

echo "Downloading latest ANGLE via git clone."
# Remove all files including hidden ones, but not . or ..
rm -rf ..?* .[!.]* *
git clone https://chromium.googlesource.com/angle/angle .
echo "Successfully downloaded latest ANGLE."
echo -n "Commit hash: "
COMMIT_HASH=`git rev-parse HEAD`
echo "$COMMIT_HASH"
echo ""

echo "Applying WebKit's local ANGLE changes to the old ANGLE version."
git checkout -B downstream-changes "$PREVIOUS_COMMIT_HASH"
pushd .. &> /dev/null
git checkout HEAD -- ANGLE
popd &> /dev/null

echo "Copying src/commit.h to src/id/commit.h"
mkdir -p src/id
git show origin/master:src/commit.h > src/id/commit.h

echo "Updating ANGLE.plist commit hashes."
sed -i.bak -e "s/\([^a-z0-9]\)[a-z0-9]\{40\}\([^a-z0-9]\)/\1$COMMIT_HASH\2/g" ANGLE.plist
echo "Updating ANGLE.plist date."
sed -i.bak -e "s/<string>[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]<\/string>/<string>`date +%Y-%m-%d`<\/string>/g" ANGLE.plist
rm ANGLE.plist.bak

echo "Translating gni build files to cmake."
git checkout origin/master -- src/compiler.gni src/libGLESv2.gni
./gni-to-cmake.py src/compiler.gni Compiler.cmake
./gni-to-cmake.py src/libGLESv2.gni GLESv2.cmake
git checkout src/compiler.gni src/libGLESv2.gni

echo "Rebasing WebKit's local changes on latest ANGLE master."
git add -A
git commit -m "WebKit changes since last ANGLE update."
git checkout -B upstream-rebased-on-webkit origin/master
if ! git rebase downstream-changes; then
    echo
    echo "There is now a git repo in Source/ThirdParty/ANGLE with a rebase in progress."
    echo "You must resolve the merge conflict and continue the rebase. Make sure to do"
    echo "this in the Source/ThirdParty/ANGLE repo, not the main WebKit repo."
    echo
    print_rebase_message_and_exit
fi
cleanup_after_successful_rebase_and_exit
