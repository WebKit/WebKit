#!/bin/sh

set -e

ARGS=("$@")

cd $SRCROOT

if [ -z "${BUILD_SCRIPTS_DIR}" ]; then
    if [ "${DEPLOYMENT_LOCATION}" == "YES" ]; then
        BUILD_SCRIPTS_DIR="${SDKROOT}${WK_ALTERNATE_WEBKIT_SDK_PATH}${WK_LIBRARY_HEADERS_FOLDER_PATH}/wtf/Scripts"
    else
        BUILD_SCRIPTS_DIR="${BUILT_PRODUCTS_DIR}${WK_LIBRARY_HEADERS_FOLDER_PATH}/wtf/Scripts"
    fi
fi

UnifiedSourceCppFileCount=530
UnifiedSourceCFileCount=0
UnifiedSourceMmFileCount=62

if [ $# -eq 0 ]; then
    echo "Using unified source list files: Sources.txt, SourcesCocoa.txt, platform/SourcesLibWebRTC.txt"
fi

SOURCES="Sources.txt SourcesCocoa.txt platform/SourcesLibWebRTC.txt"
if [ "${USE_INTERNAL_SDK}" == "YES" ]; then
    SOURCES="${SOURCES} SourcesCocoaInternalSDK.txt"
fi

/usr/bin/env ruby "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.rb" --derived-sources-path "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore" --source-tree-path "${SRCROOT}" --max-cpp-bundle-count ${UnifiedSourceCppFileCount}  --max-c-bundle-count ${UnifiedSourceCFileCount} --max-obj-c-bundle-count ${UnifiedSourceMmFileCount} --dense-bundle-filter "JS*" --dense-bundle-filter "bindings/js/*" $SOURCES "${ARGS[@]}" > /dev/null
