#!/bin/sh

if [[ "${SKIP_INSTALL}" == "YES" || "${WK_PLATFORM_NAME}" == maccatalyst || "${WK_PLATFORM_NAME}" == iosmac ]]; then
    exit 0;
fi

for (( i = 0; i < ${SCRIPT_INPUT_FILE_COUNT}; i++ )); do
    eval SRC_PLIST=\${SCRIPT_INPUT_FILE_${i}}
    eval DST_PLIST=\${SCRIPT_OUTPUT_FILE_${i}}
    DST_PLIST_DIR=$(dirname "$DST_PLIST")
    mkdir -p "$DST_PLIST_DIR"
    cp "$SRC_PLIST" "$DST_PLIST"
    plutil -convert binary1 "$DST_PLIST"
done
