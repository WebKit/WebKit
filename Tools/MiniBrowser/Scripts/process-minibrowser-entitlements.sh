#!/bin/bash
set -e

function plistbuddy()
{
    /usr/libexec/PlistBuddy -c "$*" "${WK_PROCESSED_XCENT_FILE}"
}

# ========================================
# Mac entitlements
# ========================================

function mac_process_minibrowser_entitlements()
{
    # Start with the template entitlements from MiniBrowser.entitlements
    plistbuddy Merge "${SCRIPT_INPUT_FILE_0}"
}

rm -f "${WK_PROCESSED_XCENT_FILE}"
plistbuddy Clear dict

if [[ "${WK_PLATFORM_NAME}" == macosx ]]
then
    if [[ "${PRODUCT_NAME}" == MiniBrowser ]]; then mac_process_minibrowser_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
else
    echo "Unsupported/unknown platform: ${WK_PLATFORM_NAME}"
fi

function process_additional_entitlements()
{
    local ENTITLEMENTS_SCRIPT=$1
    shift
    for PREFIX in "${@}"; do
        if [[ -f "${PREFIX}/${ENTITLEMENTS_SCRIPT}" ]]; then
            source "${PREFIX}/${ENTITLEMENTS_SCRIPT}"
            break
        fi
    done
}

ADDITIONAL_ENTITLEMENTS_SCRIPT=usr/local/include/WebKitAdditions/Scripts/process-additional-entitlements.sh
process_additional_entitlements "${ADDITIONAL_ENTITLEMENTS_SCRIPT}" "${BUILT_PRODUCTS_DIR}" "${SDKROOT}"

exit 0
