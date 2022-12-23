#!/bin/sh

set -e

ARGS=("$@")

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/DumpRenderTree"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/DumpRenderTree"

export DumpRenderTree="${SRCROOT}"
/bin/ln -sfh "${WEBCORE_PRIVATE_HEADERS_DIR}" WebCorePrivateHeaders
export WebCoreScripts="WebCorePrivateHeaders"

if [ ! "$CC" ]; then
    export CC="`xcrun -find clang`"
fi

if [ "${ACTION}" = "analyze" -o "${ACTION}" = "build" -o "${ACTION}" = "install" -o "${ACTION}" = "installhdrs" ]; then
    make -f "${DumpRenderTree}/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.activecpu` SDKROOT="${SDKROOT}" "${ARGS[@]}"
fi
