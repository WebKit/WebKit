#!/bin/sh

set -e

ARGS=("$@")

cd $SRCROOT

if [ -z "${BUILD_SCRIPTS_DIR}" ]; then
    BUILD_SCRIPTS_DIR="${WTF_BUILD_SCRIPTS_DIR}"
fi

UnifiedSourceCppFileCount=530
UnifiedSourceCFileCount=0
UnifiedSourceMmFileCount=1
UnifiedSourceNonARCMmFileCount=62
UnifiedSourceHeaderGroupFileCount=77

if [ $# -eq 0 ]; then
    echo "Using unified source list files: Sources.txt, SourcesCocoa.txt, platform/SourcesLibWebRTC.txt"
fi

SOURCES="Sources.txt SourcesCocoa.txt platform/SourcesLibWebRTC.txt"
if [ "${USE_INTERNAL_SDK}" == "YES" ]; then
    SOURCES="${SOURCES} SourcesCocoaInternalSDK.txt"
fi

/usr/bin/env python3 "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.py" --derived-sources-path "${BUILT_PRODUCTS_DIR}/DerivedSources/WebCore" --source-tree-path "${SRCROOT}" --max-cpp-bundle-count ${UnifiedSourceCppFileCount} --max-c-bundle-count ${UnifiedSourceCFileCount} --max-obj-c-bundle-count ${UnifiedSourceMmFileCount} --max-non-arc-obj-c-bundle-count ${UnifiedSourceNonARCMmFileCount} --max-header-bundle-count ${UnifiedSourceHeaderGroupFileCount} --dense-bundle-filter "JS*" --dense-bundle-filter "bindings/js/*" $SOURCES "${ARGS[@]}" > /dev/null
