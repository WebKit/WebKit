#!/bin/sh

set -e

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/JavaScriptCore"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/JavaScriptCore"

/bin/ln -sfh "${SRCROOT}" JavaScriptCore
export JavaScriptCore="JavaScriptCore"
export BUILT_PRODUCTS_DIR="../.."

make --no-builtin-rules -f "JavaScriptCore/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.ncpu`
