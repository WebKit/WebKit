#!/bin/sh

set -e

ARGS=("$@")

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore"

/bin/ln -sfh "${SRCROOT}" WebCore
export WebCore="WebCore"
/bin/ln -sfh "${JAVASCRIPTCORE_PRIVATE_HEADERS_DIR}" JavaScriptCorePrivateHeaders
export JavaScriptCore_SCRIPTS_DIR="JavaScriptCorePrivateHeaders"

if [ ! $CC ]; then
    export CC="`xcrun -find clang`"
fi

if [ ! $GPERF ]; then
    export GPERF="`xcrun -find gperf`"
fi

MAKEFILE_INCLUDE_FLAGS=$(echo "${WEBKITADDITIONS_HEADER_SEARCH_PATHS}" | perl -e 'print "-I" . join(" -I", split(" ", <>));')

if [ "${ACTION}" = "build" -o "${ACTION}" = "install" -o "${ACTION}" = "installhdrs" -o "${ACTION}" = "installapi" ]; then
    make --no-builtin-rules ${MAKEFILE_INCLUDE_FLAGS} -f "WebCore/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.activecpu` SDKROOT="${SDKROOT}" "${ARGS[@]}"
fi
