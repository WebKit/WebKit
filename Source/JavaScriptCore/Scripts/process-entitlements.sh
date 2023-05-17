#!/bin/bash

function plistbuddy()
{
    /usr/libexec/PlistBuddy -c "$*" "${WK_PROCESSED_XCENT_FILE}"
}

# ========================================
# Mac entitlements
# ========================================

function mac_process_jsc_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
        then
            plistbuddy Add :com.apple.private.verified-jit bool YES
            plistbuddy Add :com.apple.security.cs.single-jit bool YES
        fi
    fi
}

function mac_process_testapi_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.security.cs.allow-jit bool YES
        plistbuddy Add :com.apple.rootless.storage.JavaScriptCore bool YES

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
        then
            plistbuddy Add :com.apple.private.verified-jit bool YES
            plistbuddy Add :com.apple.security.cs.single-jit bool YES
        fi
    fi
}

# ========================================
# macCatalyst entitlements
# ========================================

function maccatalyst_process_jsc_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
    then
        plistbuddy Add :com.apple.private.verified-jit bool YES
        plistbuddy Add :com.apple.security.cs.single-jit bool YES
    fi
}

function maccatalyst_process_testapi_entitlements()
{
    plistbuddy Add :com.apple.rootless.storage.JavaScriptCore bool YES
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
    then
        plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
    then
        plistbuddy Add :com.apple.private.verified-jit bool YES
        plistbuddy Add :com.apple.security.cs.single-jit bool YES
    fi
}

# ========================================
# iOS Family entitlements
# ========================================

function ios_family_process_jsc_entitlements()
{
    plistbuddy Add :com.apple.private.verified-jit bool YES
    plistbuddy Add :dynamic-codesigning bool YES
}

function ios_family_process_testapi_entitlements()
{
    ios_family_process_jsc_entitlements
}

rm -f "${WK_PROCESSED_XCENT_FILE}"
plistbuddy Clear dict

if [[ "${WK_PLATFORM_NAME}" =~ .*simulator ]]
then
    [[ "${RC_XBS}" != YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES
elif [[ "${WK_PLATFORM_NAME}" == macosx ]]
then
    [[ "${RC_XBS}" != YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == jsc ||
          "${PRODUCT_NAME}" == dynbench ||
          "${PRODUCT_NAME}" == minidom ||
          "${PRODUCT_NAME}" == testair ||
          "${PRODUCT_NAME}" == testb3 ||
          "${PRODUCT_NAME}" == testdfg ||
          "${PRODUCT_NAME}" == testmasm ||
          "${PRODUCT_NAME}" == testmem ||
          "${PRODUCT_NAME}" == testRegExp ]]; then mac_process_jsc_entitlements
    elif [[ "${PRODUCT_NAME}" == testapi ]]; then mac_process_testapi_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
elif [[ "${WK_PLATFORM_NAME}" == maccatalyst || "${WK_PLATFORM_NAME}" == iosmac ]]
then
    [[ "${RC_XBS}" != YES && "${PRODUCT_NAME}" == jsc ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == jsc ||
          "${PRODUCT_NAME}" == dynbench ||
          "${PRODUCT_NAME}" == minidom ||
          "${PRODUCT_NAME}" == testair ||
          "${PRODUCT_NAME}" == testb3 ||
          "${PRODUCT_NAME}" == testdfg ||
          "${PRODUCT_NAME}" == testmasm ||
          "${PRODUCT_NAME}" == testmem ||
          "${PRODUCT_NAME}" == testRegExp ]]; then maccatalyst_process_jsc_entitlements
    elif [[ "${PRODUCT_NAME}" == testapi ]]; then maccatalyst_process_testapi_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
elif [[ "${WK_PLATFORM_NAME}" == iphoneos ||
        "${WK_PLATFORM_NAME}" == appletvos ||
        "${WK_PLATFORM_NAME}" == watchos ]]
then
    if [[ "${PRODUCT_NAME}" == jsc ||
          "${PRODUCT_NAME}" == dynbench ||
          "${PRODUCT_NAME}" == minidom ||
          "${PRODUCT_NAME}" == testair ||
          "${PRODUCT_NAME}" == testb3 ||
          "${PRODUCT_NAME}" == testdfg ||
          "${PRODUCT_NAME}" == testmasm ||
          "${PRODUCT_NAME}" == testmem ||
          "${PRODUCT_NAME}" == testRegExp ]]; then ios_family_process_jsc_entitlements
    elif [[ "${PRODUCT_NAME}" == testapi ]]; then ios_family_process_testapi_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
else
    echo "Unsupported/unknown platform: ${WK_PLATFORM_NAME}"
fi

exit 0
