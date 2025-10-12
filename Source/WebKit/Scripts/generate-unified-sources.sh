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

if [ $# -eq 0 ]; then
    echo "Using unified source list files: Sources.txt, SourcesCocoa.txt, Platform/Sources.txt, Platform/SourcesCocoa.txt"
fi

UnifiedSourceCppFileCount=180
UnifiedSourceCFileCount=0
UnifiedSourceMmFileCount=16
UnifiedSourceNonARCMmFileCount=80

/usr/bin/env ruby "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.rb" --derived-sources-path "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKit" --source-tree-path "${SRCROOT}" --max-cpp-bundle-count ${UnifiedSourceCppFileCount} --max-c-bundle-count ${UnifiedSourceCFileCount} --max-obj-c-bundle-count ${UnifiedSourceMmFileCount} --max-non-arc-obj-c-bundle-count ${UnifiedSourceNonARCMmFileCount} Sources.txt SourcesCocoa.txt "${ARGS[@]}" > /dev/null

UnifiedSourceCppFileCount=5
UnifiedSourceCFileCount=0
UnifiedSourceMmFileCount=2
UnifiedSourceNonARCMmFileCount=3

/usr/bin/env ruby "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.rb" --derived-sources-path "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKit" --source-tree-path "${SRCROOT}" --max-cpp-bundle-count ${UnifiedSourceCppFileCount} --max-c-bundle-count ${UnifiedSourceCFileCount} --max-obj-c-bundle-count ${UnifiedSourceMmFileCount} --max-non-arc-obj-c-bundle-count ${UnifiedSourceNonARCMmFileCount} --bundle-filename-prefix Platform Platform/Sources.txt Platform/SourcesCocoa.txt "${ARGS[@]}" > /dev/null
