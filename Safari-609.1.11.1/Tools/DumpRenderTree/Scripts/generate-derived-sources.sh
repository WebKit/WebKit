#!/bin/sh

set -e

ARGS=("$@")

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/DumpRenderTree"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/DumpRenderTree"

export DumpRenderTree="${SRCROOT}"
export WebCoreScripts="${WEBCORE_PRIVATE_HEADERS_DIR}"

if [ ! $CC ]; then
    export CC="`xcrun -find clang`"
fi

if [ "${ACTION}" = "build" -o "${ACTION}" = "install" -o "${ACTION}" = "installhdrs" ]; then
    make -f "${DumpRenderTree}/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.activecpu` "${ARGS[@]}"
fi
