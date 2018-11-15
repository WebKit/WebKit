#!/bin/sh

set -e

cd $SRCROOT

if [ "${DEPLOYMENT_LOCATION}" == "YES" ]; then
    BUILD_SCRIPTS_DIR="${SDKROOT}${WK_ALTERNATE_WEBKIT_SDK_PATH}/usr/local/include/wtf/Scripts"
else
    BUILD_SCRIPTS_DIR="${BUILT_PRODUCTS_DIR}/usr/local/include/wtf/Scripts"
fi

UnifiedSourceCppFileCount=145
UnifiedSourceMmFileCount=5

/usr/bin/env ruby "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.rb" "--derived-sources-path" "${BUILT_PRODUCTS_DIR}/DerivedSources/JavaScriptCore" "--source-tree-path" "${SRCROOT}" "--max-cpp-bundle-count" "${UnifiedSourceCppFileCount}" "--max-obj-c-bundle-count" "${UnifiedSourceMmFileCount}" "Sources.txt" "SourcesCocoa.txt" > /dev/null
