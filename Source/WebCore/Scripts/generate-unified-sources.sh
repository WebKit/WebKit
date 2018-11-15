#!/bin/sh

set -e

cd $SRCROOT

if [ "${DEPLOYMENT_LOCATION}" == "YES" ]; then
    BUILD_SCRIPTS_DIR="${SDKROOT}${WK_ALTERNATE_WEBKIT_SDK_PATH}/usr/local/include/wtf/Scripts"
else
    BUILD_SCRIPTS_DIR="${BUILT_PRODUCTS_DIR}/usr/local/include/wtf/Scripts"
fi

UnifiedSourceCppFileCount=530
UnifiedSourceMmFileCount=62

echo "Using unified source list files: Sources.txt, SourcesCocoa.txt"

/usr/bin/env ruby "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.rb" "--derived-sources-path" "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore" "--source-tree-path" "${SRCROOT}" "--feature-flags" "${FEATURE_DEFINES}" "--max-cpp-bundle-count" "${UnifiedSourceCppFileCount}" "--max-obj-c-bundle-count" "${UnifiedSourceMmFileCount}" "Sources.txt" "SourcesCocoa.txt" > /dev/null
