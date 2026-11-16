#!/bin/sh

set -e

ARGS=("$@")

cd $SRCROOT

if [ -z "${BUILD_SCRIPTS_DIR}" ]; then
    BUILD_SCRIPTS_DIR="${WTF_BUILD_SCRIPTS_DIR}"
fi

if [ $# -eq 0 ]; then
    echo "Using unified source list files: Sources.txt, SourcesCocoa.txt, Platform/Sources.txt, Platform/SourcesCocoa.txt"
fi

UnifiedSourceCppFileCount=180
UnifiedSourceCFileCount=0
UnifiedSourceMmFileCount=16
UnifiedSourceNonARCMmFileCount=80

/usr/bin/env python3 "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.py" --derived-sources-path "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKit" --source-tree-path "${SRCROOT}" --max-cpp-bundle-count ${UnifiedSourceCppFileCount} --max-c-bundle-count ${UnifiedSourceCFileCount} --max-obj-c-bundle-count ${UnifiedSourceMmFileCount} --max-non-arc-obj-c-bundle-count ${UnifiedSourceNonARCMmFileCount} Sources.txt SourcesCocoa.txt "${ARGS[@]}" > /dev/null

UnifiedSourceCppFileCount=5
UnifiedSourceCFileCount=0
UnifiedSourceMmFileCount=2
UnifiedSourceNonARCMmFileCount=3

/usr/bin/env python3 "${BUILD_SCRIPTS_DIR}/generate-unified-source-bundles.py" --derived-sources-path "${BUILT_PRODUCTS_DIR}/DerivedSources/WebKit" --source-tree-path "${SRCROOT}" --max-cpp-bundle-count ${UnifiedSourceCppFileCount} --max-c-bundle-count ${UnifiedSourceCFileCount} --max-obj-c-bundle-count ${UnifiedSourceMmFileCount} --max-non-arc-obj-c-bundle-count ${UnifiedSourceNonARCMmFileCount} --bundle-filename-prefix Platform Platform/Sources.txt Platform/SourcesCocoa.txt "${ARGS[@]}" > /dev/null
