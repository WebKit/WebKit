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
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.private.pac.exception bool YES

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
            plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 )) && [[ -z "${SKIP_ROSETTA_BREAKING_ENTITLEMENTS}" ]]
        then
            plistbuddy Add :com.apple.private.verified-jit bool YES
            plistbuddy Add :com.apple.security.cs.single-jit bool YES
        fi
    fi
}

function mac_process_testapi_entitlements()
{
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.private.pac.exception bool YES
        plistbuddy Add :com.apple.security.cs.allow-jit bool YES
        plistbuddy Add :com.apple.rootless.storage.JavaScriptCore bool YES

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
            plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 )) && [[ -z "${SKIP_ROSETTA_BREAKING_ENTITLEMENTS}" ]]
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
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.private.pac.exception bool YES
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
            plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
        fi
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 )) && [[ -z "${SKIP_ROSETTA_BREAKING_ENTITLEMENTS}" ]]
    then
        plistbuddy Add :com.apple.private.verified-jit bool YES
        plistbuddy Add :com.apple.security.cs.single-jit bool YES
    fi
}

function maccatalyst_process_testapi_entitlements()
{
    plistbuddy Add :com.apple.rootless.storage.JavaScriptCore bool YES
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.private.pac.exception bool YES
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
    then
        plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
    fi

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 )) && [[ -z "${SKIP_ROSETTA_BREAKING_ENTITLEMENTS}" ]]
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
    plistbuddy Add :com.apple.private.pac.exception bool YES
    if [[ "${PLATFORM_NAME}" != watchos && "${PLATFORM_NAME}" != appletvos ]]; then
        plistbuddy Add :com.apple.private.verified-jit bool YES
        if [[ "${PLATFORM_NAME}" == iphoneos ]]; then
            if (( $(( ${SDK_VERSION_ACTUAL} )) >= 170400 )); then
                plistbuddy Add :com.apple.developer.cs.allow-jit bool YES
                plistbuddy Add :com.apple.developer.web-browser-engine.webcontent bool YES
            else
                plistbuddy Add :dynamic-codesigning bool YES
            fi
        else
            plistbuddy Add :dynamic-codesigning bool YES
        fi
    fi
    plistbuddy Add :com.apple.developer.kernel.extended-virtual-addressing bool YES
    plistbuddy Add :com.apple.security.fatal-exceptions array
    plistbuddy Add :com.apple.security.fatal-exceptions:0 string jit
}

rm -f "${WK_PROCESSED_XCENT_FILE}"
plistbuddy Clear dict

if [[ "${WK_PLATFORM_NAME}" =~ .*simulator ]]
then
    [[ "${RC_XBS}" != YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES
elif [[ "${WK_PLATFORM_NAME}" == macosx ]]
then
    [[ "${RC_XBS}" != YES ]] && [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

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
        "${WK_PLATFORM_NAME}" == watchos ||
        "${WK_PLATFORM_NAME}" == xros ]]
then
    if [[ "${PRODUCT_NAME}" == jsc ||
          "${PRODUCT_NAME}" == dynbench ||
          "${PRODUCT_NAME}" == minidom ||
          "${PRODUCT_NAME}" == testapi ||
          "${PRODUCT_NAME}" == testair ||
          "${PRODUCT_NAME}" == testb3 ||
          "${PRODUCT_NAME}" == testdfg ||
          "${PRODUCT_NAME}" == testmasm ||
          "${PRODUCT_NAME}" == testmem ||
          "${PRODUCT_NAME}" == testRegExp ]]; then ios_family_process_jsc_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
else
    echo "Unsupported/unknown platform: ${WK_PLATFORM_NAME}"
fi

exit 0
