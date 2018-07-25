#!/bin/sh
set -e

PROCESSED_XCENT_FILE="${TEMP_FILE_DIR}/${FULL_PRODUCT_NAME}.xcent"

if [[ ${WK_PLATFORM_NAME} == "macosx" ]]; then

    if [[ ${WK_USE_RESTRICTED_ENTITLEMENTS} == "YES" ]]; then
        echo "Processing restricted entitlements for Internal SDK";

        if (( ${TARGET_MAC_OS_X_VERSION_MAJOR} >= 101400 )); then
            echo "Adding macOS platform entitlements.";
            /usr/libexec/PlistBuddy -c "Merge Configurations/WebContent-or-Plugin-OSX-restricted.entitlements" "${PROCESSED_XCENT_FILE}";
        fi

        if [[ ${WK_WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT} == "YES" ]]; then
            echo "Adding domain extension entitlement for relocatable build.";
            /usr/libexec/PlistBuddy -c "Add :com.apple.private.xpc.domain-extension bool YES" "${PROCESSED_XCENT_FILE}";
        fi
    fi

    if [[ ${WK_XPC_SERVICE_VARIANT} == "Development" ]]; then
        echo "Disabling library validation for development build.";
        /usr/libexec/PlistBuddy -c "Add :com.apple.security.cs.disable-library-validation bool YES" "${PROCESSED_XCENT_FILE}";
    fi
fi
