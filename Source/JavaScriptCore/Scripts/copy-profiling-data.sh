#!/bin/sh

# Decompresses and copies PGO profiles from WebKitAdditions to a derived folder.

if [ "${CLANG_USE_OPTIMIZATION_PROFILE}" = YES ]; then
    compression_tool -v -decode -i "${SCRIPT_INPUT_FILE_0}" -o "${SCRIPT_OUTPUT_FILE_0}" && exit
    if [ "${CONFIGURATION}" = Production ]; then
        echo "error: ${SCRIPT_INPUT_FILE_0} failed to extract"
        exit 1
    else
        echo "warning: ${SCRIPT_INPUT_FILE_0} failed to extract, falling back to stub profile data"
        cp -vf "${SCRIPT_INPUT_FILE_1}" "${SCRIPT_OUTPUT_FILE_0}"
    fi
fi
