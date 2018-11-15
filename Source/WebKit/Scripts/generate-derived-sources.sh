#!/bin/sh

set -e

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKit2"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKit2"

export WebKit2="${SRCROOT}"

/bin/ln -sfh "${JAVASCRIPTCORE_PRIVATE_HEADERS_DIR}" JavaScriptCorePrivateHeaders
export JavaScriptCore_SCRIPTS_DIR="JavaScriptCorePrivateHeaders"

if [ ! $CC ]; then
    export CC="`xcrun -find clang`"
fi

MAKEFILE_INCLUDE_FLAGS=$(echo "${WEBKITADDITIONS_HEADER_SEARCH_PATHS}" | perl -e 'print "-I" . join(" -I", split(" ", <>));')

if [ "${ACTION}" = "build" -o "${ACTION}" = "install" -o "${ACTION}" = "installhdrs" -o "${ACTION}" = "installapi" ]; then
    make --no-builtin-rules ${MAKEFILE_INCLUDE_FLAGS} -f "${WebKit2}/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.activecpu` SDKROOT=${SDKROOT}
fi
