#!/bin/sh
set -e

PROCESSED_XCENT_FILE="${TEMP_FILE_DIR}/${FULL_PRODUCT_NAME}.xcent"

if [[ ${WK_PLATFORM_NAME} == "macosx" ]]; then

    if [[ ${WK_USE_RESTRICTED_ENTITLEMENTS} == "YES" ]]; then
        echo "Processing restricted entitlements for Internal SDK";

        echo "Adding sandbox entitlements for Network process.";
        /usr/libexec/PlistBuddy -c "Merge Configurations/Network-OSX-sandbox.entitlements" "${PROCESSED_XCENT_FILE}";
    fi
fi
