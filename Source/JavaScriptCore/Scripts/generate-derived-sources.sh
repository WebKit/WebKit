#!/bin/sh

set -e

ARGS=("$@")

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/JavaScriptCore"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/JavaScriptCore"

/bin/ln -sfh "${SRCROOT}" JavaScriptCore
export JavaScriptCore="JavaScriptCore"
export BUILT_PRODUCTS_DIR="../.."

if [ ! $CC ]; then
    export CC="`xcrun -find clang`"
fi

make --no-builtin-rules -f "JavaScriptCore/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.ncpu` "${ARGS[@]}"
