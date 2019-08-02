#!/bin/bash

function plistbuddy()
{
    /usr/libexec/PlistBuddy -c "$*" "${WK_PROCESSED_XCENT_FILE}"
}

# ========================================
# Mac entitlements
# ========================================

function mac_process_webcontent_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.rootless.storage.WebKitWebContentSandbox bool YES
    fi

    mac_process_webcontent_or_plugin_entitlements
}

function mac_process_network_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 101500 ))
        then
            plistbuddy Add :com.apple.private.network.socket-delegate bool YES
        fi

        plistbuddy Add :com.apple.rootless.storage.WebKitNetworkingSandbox bool YES
    fi
}

function mac_process_plugin_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit                        bool YES
    plistbuddy Add :com.apple.security.cs.allow-unsigned-executable-memory bool YES
    plistbuddy Add :com.apple.security.cs.disable-library-validation       bool YES
    plistbuddy Add :com.apple.security.files.user-selected.read-write      bool YES
    plistbuddy Add :com.apple.security.print                               bool YES

    mac_process_webcontent_or_plugin_entitlements
}

function mac_process_webcontent_or_plugin_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 101400 ))
        then
            plistbuddy Add :com.apple.tcc.delegated-services array
            plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone
            plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
        fi

        if [[ "${WK_WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT}" == YES ]]
        then
            plistbuddy Add :com.apple.private.xpc.domain-extension bool YES
        fi
    fi

    if [[ "${WK_XPC_SERVICE_VARIANT}" == Development ]]
    then
        plistbuddy Add :com.apple.security.cs.disable-library-validation bool YES
    fi
}

# ========================================
# macCatalyst entitlements
# ========================================

function maccatalyst_process_webcontent_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES
}

function maccatalyst_process_network_entitlements()
{
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.security.network.client bool YES
}

function maccatalyst_process_plugin_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit                        bool YES
    plistbuddy Add :com.apple.security.cs.allow-unsigned-executable-memory bool YES
    plistbuddy Add :com.apple.security.cs.disable-library-validation       bool YES
    plistbuddy Add :com.apple.security.files.user-selected.read-write      bool YES
    plistbuddy Add :com.apple.security.print                               bool YES
}


# ========================================
# iOS Family entitlements
# ========================================

function ios_family_process_webcontent_entitlements()
{
    plistbuddy Add :com.apple.QuartzCore.secure-mode bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy Add :com.apple.locationd.authorizeapplications bool YES
    plistbuddy Add :com.apple.locationd.effective_bundle bool YES
    plistbuddy Add :com.apple.private.allow-explicit-graphics-priority bool YES
    plistbuddy Add :com.apple.private.coremedia.extensions.audiorecording.allow bool YES
    plistbuddy Add :com.apple.private.coremedia.pidinheritance.allow bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.private.webinspector.allow-remote-inspection bool YES
    plistbuddy Add :com.apple.private.webinspector.proxy-application bool YES
    plistbuddy Add :dynamic-codesigning bool YES

    plistbuddy Add :com.apple.tcc.delegated-services array
    plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
    plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone

    plistbuddy Add :seatbelt-profiles array
    plistbuddy Add :seatbelt-profiles:0 string com.apple.WebKit.WebContent
}

function ios_family_process_network_entitlements()
{
    plistbuddy Add :com.apple.multitasking.systemappassertions bool YES
    plistbuddy Add :com.apple.payment.all-access bool YES
    plistbuddy Add :com.apple.private.accounts.bundleidspoofing bool YES
    plistbuddy Add :com.apple.private.dmd.policy bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES

    plistbuddy Add :seatbelt-profiles array
    plistbuddy Add :seatbelt-profiles:0 string com.apple.WebKit.Networking
}

function ios_family_process_plugin_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit                        bool YES
    plistbuddy Add :com.apple.security.cs.allow-unsigned-executable-memory bool YES
    plistbuddy Add :com.apple.security.cs.disable-library-validation       bool YES
    plistbuddy Add :com.apple.security.files.user-selected.read-write      bool YES
    plistbuddy Add :com.apple.security.print                               bool YES
}


rm -f "${WK_PROCESSED_XCENT_FILE}"
plistbuddy Clear dict

if [[ "${WK_PLATFORM_NAME}" =~ .*simulator ]]
then
    [[ "${RC_XBS}" != YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES
elif [[ "${WK_PLATFORM_NAME}" == macosx ]]
then
    [[ "${RC_XBS}" != YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then mac_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then mac_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then mac_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Plugin.64 ]]; then mac_process_plugin_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
elif [[ "${WK_PLATFORM_NAME}" == maccatalyst || "${WK_PLATFORM_NAME}" == iosmac ]]
then
    [[ "${RC_XBS}" != YES && ( "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development || "${PRODUCT_NAME}" == com.apple.WebKit.Plugin.64 ) ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then maccatalyst_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then maccatalyst_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then maccatalyst_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Plugin.64 ]]; then maccatalyst_process_plugin_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
elif [[ "${WK_PLATFORM_NAME}" == iphoneos ||
        "${WK_PLATFORM_NAME}" == appletvos ||
        "${WK_PLATFORM_NAME}" == watchos ]]
then
    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then true
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then ios_family_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then ios_family_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Plugin.64 ]]; then ios_family_process_plugin_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
else
    echo "Unsupported/unknown platform: ${WK_PLATFORM_NAME}"
fi

exit 0
