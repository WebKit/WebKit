#!/bin/sh

for (( i = 0; i < ${SCRIPT_INPUT_FILE_COUNT}; i++ )); do
    eval SRC_PLIST=\${SCRIPT_INPUT_FILE_${i}}
    eval DST_PLIST=\${SCRIPT_OUTPUT_FILE_${i}}

    if [[ -n "$DST_PLIST" ]]; then
        DST_PLIST_DIR=$(dirname "$DST_PLIST")
        mkdir -p "$DST_PLIST_DIR"
        cp "$SRC_PLIST" "$DST_PLIST"
        plutil -convert binary1 "$DST_PLIST"
    fi
done
