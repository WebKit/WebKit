#!/bin/sh

set -e

ARGS=("$@")

cd $SRCROOT

if [ -z "${BUILD_SCRIPTS_DIR}" ]; then
    if [ "${DEPLOYMENT_LOCATION}" == "YES" ]; then
        BUILD_SCRIPTS_DIR="${SDKROOT}${WK_ALTERNATE_WEBKIT_SDK_PATH}/usr/local/include/wtf/Scripts"
    else
        BUILD_SCRIPTS_DIR="${BUILT_PRODUCTS_DIR}/usr/local/include/wtf/Scripts"
    fi
fi

UnifiedSourceCppFileCount=530
UnifiedSourceMmFileCount=62

if [ $# -eq 0 ]; then
    echo "Using unified source list files: Sources.txt, SourcesCocoa.txt"
fi

if [ ! $CC ]; then
    export CC="`xcrun -find clang`"
fi

if [ -n "$SDKROOT" ]; then
    SDK_FLAGS="-isysroot ${SDKROOT}"
fi

if [ "${USE_LLVM_TARGET_TRIPLES_FOR_CLANG}" = "YES" ]; then
    # FIXME: This is probably wrong for fat builds, but matches the current behavior of DerivedSources.make
    WK_CURRENT_ARCH=$(echo ${ARCHS} | cut -d " " -f1)
    TARGET_TRIPLE_FLAGS="-target ${WK_CURRENT_ARCH}-${LLVM_TARGET_TRIPLE_VENDOR}-${LLVM_TARGET_TRIPLE_OS_VERSION}${LLVM_TARGET_TRIPLE_SUFFIX}"
fi

FRAMEWORK_FLAGS=$(echo ${BUILT_PRODUCTS_DIR} ${FRAMEWORK_SEARCH_PATHS} ${SYSTEM_FRAMEWORK_SEARCH_PATHS} | perl -e 'print "-F " . join(" -F ", split(" ", <>));')
HEADER_FLAGS=$(echo ${BUILT_PRODUCTS_DIR} ${HEADER_SEARCH_PATHS} ${SYSTEM_HEADER_SEARCH_PATHS} | perl -e 'print "-I" . join(" -I", split(" ", <>));')

FEATURE_DEFINES_FROM_XCCONFIG=$(echo ${FEATURE_DEFINES} | perl -e 'print "-D" . join(" -D", split(" ", <>));')
ENABLED_FEATURES=$(${CC} -std=${CLANG_CXX_LANGUAGE_STANDARD} -x c++ -E -P -dM ${SDK_FLAGS} ${TARGET_TRIPLE_FLAGS} ${FEATURE_DEFINES_FROM_XCCONFIG} ${FRAMEWORK_FLAGS} ${HEADER_FLAGS} -include "wtf/Platform.h" /dev/null | grep '\#define ENABLE_.* 1' | cut -d' ' -f2)

/usr/bin/env ruby "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.rb" "--derived-sources-path" "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore" "--source-tree-path" "${SRCROOT}" "--feature-flags" "${ENABLED_FEATURES}" "--max-cpp-bundle-count" "${UnifiedSourceCppFileCount}" "--max-obj-c-bundle-count" "${UnifiedSourceMmFileCount}" "--dense-bundle-filter" "JS*" "--dense-bundle-filter" "bindings/js/*" "Sources.txt" "SourcesCocoa.txt" "${ARGS[@]}" > /dev/null
