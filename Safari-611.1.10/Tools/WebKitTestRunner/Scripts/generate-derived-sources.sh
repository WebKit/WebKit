#!/bin/sh

set -e

ARGS=("$@")

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKitTestRunner"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKitTestRunner"

export WebKitTestRunner="${SRCROOT}"
export WebCoreScripts="${WEBCORE_PRIVATE_HEADERS_DIR}"

if [ ! $CC ]; then
    export CC="`xcrun -find clang`"
fi

if [ "${ACTION}" = "build" -o "${ACTION}" = "install" -o "${ACTION}" = "installhdrs" ]; then
    make -f "${WebKitTestRunner}/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.activecpu` "${ARGS[@]}"
fi
