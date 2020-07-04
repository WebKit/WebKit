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

SOURCES="Sources.txt SourcesCocoa.txt"
if [ "${USE_INTERNAL_SDK}" == "YES" ]; then
    SOURCES="${SOURCES} SourcesCocoaInternalSDK.txt"
fi

/usr/bin/env ruby "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.rb" --derived-sources-path "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore" --source-tree-path "${SRCROOT}" --max-cpp-bundle-count ${UnifiedSourceCppFileCount} --max-obj-c-bundle-count ${UnifiedSourceMmFileCount} --dense-bundle-filter "JS*" --dense-bundle-filter "bindings/js/*" $SOURCES "${ARGS[@]}" > /dev/null
