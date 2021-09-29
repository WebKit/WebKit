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
        plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
        plistbuddy Add :com.apple.rootless.storage.WebKitWebContentSandbox bool YES
        plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.developer.videotoolbox.client-sandboxed-decoder bool YES
            plistbuddy Add :com.apple.pac.shared_region_id string WebContent
            plistbuddy Add :com.apple.private.pac.exception bool YES
            plistbuddy Add :com.apple.private.security.message-filter bool YES
            plistbuddy Add :com.apple.avfoundation.allow-system-wide-context bool YES
            plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
        fi
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
        then
            plistbuddy add :com.apple.coreaudio.allow-vorbis-decode YES
        fi
    fi

    mac_process_webcontent_or_plugin_entitlements
}

function mac_process_gpu_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 101400 ))
        then
            plistbuddy Add :com.apple.tcc.delegated-services array
            plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone
            plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.developer.videotoolbox.client-sandboxed-decoder bool YES
            plistbuddy Add :com.apple.avfoundation.allow-system-wide-context bool YES
            plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        plistbuddy Add :com.apple.private.memory.ownership_transfer bool YES
        plistbuddy Add :com.apple.rootless.storage.WebKitGPUSandbox bool YES
        plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    fi
}

function mac_process_webauthn_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        plistbuddy Add :com.apple.security.device.usb bool YES

        plistbuddy Add :keychain-access-groups array
        plistbuddy Add :keychain-access-groups:0 string com.apple.webkit.webauthn
        plistbuddy Add :keychain-access-groups:1 string lockdown-identities

        plistbuddy Add :com.apple.security.attestation.access bool YES
        plistbuddy Add :com.apple.keystore.sik.access bool YES
        plistbuddy Add :com.apple.private.RemoteServiceDiscovery.allow-sandbox bool YES
        plistbuddy Add :com.apple.private.RemoteServiceDiscovery.device-admin bool YES
        plistbuddy Add :com.apple.appattest.spi bool YES
        plistbuddy Add :com.apple.mobileactivationd.spi bool YES
        plistbuddy Add :com.apple.mobileactivationd.bridge bool YES
        plistbuddy Add :com.apple.private.security.bootpolicy bool YES

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi
}

function mac_process_network_entitlements()
{
    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 101500 ))
        then
            plistbuddy Add :com.apple.private.network.socket-delegate bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token array
            plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:0 string kTCCServiceWebKitIntelligentTrackingPrevention
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        plistbuddy Add :com.apple.private.launchservices.allowedtochangethesekeysinotherapplications array
        plistbuddy Add :com.apple.private.launchservices.allowedtochangethesekeysinotherapplications:0 string LSActivePageUserVisibleOriginsKey
        plistbuddy Add :com.apple.private.launchservices.allowedtochangethesekeysinotherapplications:1 string LSDisplayName
        plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
        plistbuddy Add :com.apple.rootless.storage.WebKitNetworkingSandbox bool YES
        plistbuddy Add :com.apple.symptom_analytics.configure bool YES
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

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi

        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 120000 ))
        then
            plistbuddy Add :com.apple.private.verified-jit bool YES
            plistbuddy Add :com.apple.security.cs.single-jit bool YES
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
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES

    if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
    then
        plistbuddy Add :com.apple.pac.shared_region_id string WebContent
        plistbuddy Add :com.apple.private.pac.exception bool YES
        plistbuddy Add :com.apple.private.security.message-filter bool YES
        plistbuddy Add :com.apple.UIKit.view-service-wants-custom-idiom-and-scale bool YES
        plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
    fi

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

function maccatalyst_process_gpu_entitlements()
{
    plistbuddy Add :com.apple.security.network.client bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy Add :com.apple.private.memory.ownership_transfer bool YES
    plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi
}

function maccatalyst_process_network_entitlements()
{
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.security.network.client bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES

    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token array
    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:0 string kTCCServiceWebKitIntelligentTrackingPrevention

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        if (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 110000 ))
        then
            plistbuddy Add :com.apple.security.cs.jit-write-allowlist bool YES
        fi
    fi
}

function maccatalyst_process_plugin_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit                        bool YES
    plistbuddy Add :com.apple.security.cs.allow-unsigned-executable-memory bool YES
    plistbuddy Add :com.apple.security.cs.disable-library-validation       bool YES
    plistbuddy Add :com.apple.security.files.user-selected.read-write      bool YES
    plistbuddy Add :com.apple.security.print                               bool YES

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


# ========================================
# iOS Family entitlements
# ========================================

function ios_family_process_webcontent_entitlements()
{
    plistbuddy Add :com.apple.QuartzCore.secure-mode bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
    plistbuddy Add :com.apple.developer.coremedia.allow-alternate-video-decoder-selection bool YES
    plistbuddy Add :com.apple.mediaremote.set-playback-state bool YES
    plistbuddy Add :com.apple.pac.shared_region_id string WebContent
    plistbuddy Add :com.apple.private.allow-explicit-graphics-priority bool YES
    plistbuddy Add :com.apple.private.coremedia.extensions.audiorecording.allow bool YES
    plistbuddy Add :com.apple.private.coremedia.pidinheritance.allow bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.private.pac.exception bool YES
    plistbuddy Add :com.apple.private.verified-jit bool YES
    plistbuddy Add :com.apple.private.security.message-filter bool YES
    plistbuddy Add :com.apple.private.webinspector.allow-remote-inspection bool YES
    plistbuddy Add :com.apple.private.webinspector.proxy-application bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES
    plistbuddy Add :dynamic-codesigning bool YES

    plistbuddy Add :com.apple.tcc.delegated-services array
    plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
    plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone

    plistbuddy Add :seatbelt-profiles array
    plistbuddy Add :seatbelt-profiles:0 string com.apple.WebKit.WebContent
}

function ios_family_process_gpu_entitlements()
{
    plistbuddy Add :com.apple.QuartzCore.secure-mode bool YES
    plistbuddy Add :com.apple.QuartzCore.webkit-end-points bool YES
    plistbuddy add :com.apple.QuartzCore.webkit-limited-types bool YES
    plistbuddy Add :com.apple.developer.coremedia.allow-alternate-video-decoder-selection bool YES
    plistbuddy Add :com.apple.mediaremote.set-playback-state bool YES
    plistbuddy Add :com.apple.private.allow-explicit-graphics-priority bool YES
    plistbuddy Add :com.apple.private.coremedia.extensions.audiorecording.allow bool YES
    plistbuddy Add :com.apple.private.mediaexperience.startrecordinginthebackground.allow bool YES
    plistbuddy Add :com.apple.private.coremedia.pidinheritance.allow bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.memory.ownership_transfer bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES

    plistbuddy Add :com.apple.tcc.delegated-services array
    plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
    plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone

    plistbuddy Add :seatbelt-profiles array
    plistbuddy Add :seatbelt-profiles:0 string com.apple.WebKit.GPU

    plistbuddy Add :com.apple.systemstatus.activityattribution bool YES
    plistbuddy Add :com.apple.security.exception.mach-lookup.global-name array
    plistbuddy Add :com.apple.security.exception.mach-lookup.global-name:0 string com.apple.systemstatus.activityattribution
}

function ios_family_process_webauthn_entitlements()
{
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES

    plistbuddy Add :com.apple.security.device.usb bool YES

    plistbuddy Add :com.apple.private.tcc.allow array
    plistbuddy Add :com.apple.private.tcc.allow:0 string kTCCServiceListenEvent

    plistbuddy Add :com.apple.security.application-groups array
    plistbuddy Add :com.apple.security.application-groups:0 string group.com.apple.webkit

    plistbuddy Add :com.apple.nfcd.hwmanager bool YES
    plistbuddy Add :com.apple.nfcd.session.reader.internal bool YES
    # FIXME(rdar://problem/72646664): Find a better way to invoke NearField in the background.
    plistbuddy Add :com.apple.internal.nfc.allow.backgrounded.session bool YES
    plistbuddy Add :com.apple.UIKit.vends-view-services bool YES

    plistbuddy Add :keychain-access-groups array
    plistbuddy Add :keychain-access-groups:0 string com.apple.webkit.webauthn
    plistbuddy Add :keychain-access-groups:1 string lockdown-identities

    plistbuddy Add :com.apple.private.MobileGestalt.AllowedProtectedKeys array
    plistbuddy Add :com.apple.private.MobileGestalt.AllowedProtectedKeys:0 string UniqueChipID
    plistbuddy Add :com.apple.private.MobileGestalt.AllowedProtectedKeys:1 string SerialNumber

    plistbuddy Add :com.apple.security.system-groups array
    plistbuddy Add :com.apple.security.system-groups:0 string systemgroup.com.apple.mobileactivationd

    plistbuddy Add :com.apple.security.attestation.access bool YES
    plistbuddy Add :com.apple.keystore.sik.access bool YES
    plistbuddy Add :com.apple.appattest.spi bool YES
    plistbuddy Add :com.apple.mobileactivationd.spi bool YES

    plistbuddy Add :com.apple.springboard.remote-alert bool YES
    plistbuddy Add :com.apple.frontboard.launchapplications bool YES

    plistbuddy Add :seatbelt-profiles array
    plistbuddy Add :seatbelt-profiles:0 string com.apple.WebKit.WebAuthn
}

function ios_family_process_network_entitlements()
{
    plistbuddy Add :com.apple.multitasking.systemappassertions bool YES
    plistbuddy Add :com.apple.payment.all-access bool YES
    plistbuddy Add :com.apple.private.accounts.bundleidspoofing bool YES
    plistbuddy Add :com.apple.private.dmd.policy bool YES
    plistbuddy Add :com.apple.private.memorystatus bool YES
    plistbuddy Add :com.apple.private.network.socket-delegate bool YES
    plistbuddy Add :com.apple.private.webkit.use-xpc-endpoint bool YES
    plistbuddy Add :com.apple.runningboard.assertions.webkit bool YES

    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token array
    plistbuddy Add :com.apple.private.tcc.manager.check-by-audit-token:0 string kTCCServiceWebKitIntelligentTrackingPrevention

    plistbuddy Add :seatbelt-profiles array
    plistbuddy Add :seatbelt-profiles:0 string com.apple.WebKit.Networking
    plistbuddy Add :com.apple.symptom_analytics.configure bool YES
}

function ios_family_process_plugin_entitlements()
{
    plistbuddy Add :com.apple.private.verified-jit                         bool YES
    plistbuddy Add :com.apple.security.cs.allow-jit                        bool YES
    plistbuddy Add :com.apple.security.cs.allow-unsigned-executable-memory bool YES
    plistbuddy Add :com.apple.security.cs.disable-library-validation       bool YES
    plistbuddy Add :com.apple.security.files.user-selected.read-write      bool YES
    plistbuddy Add :com.apple.security.print                               bool YES
}


rm -f "${WK_PROCESSED_XCENT_FILE}"
plistbuddy Clear dict

# Simulator entitlements should be added to Resources/ios/XPCService-ios-simulator.entitlements
if [[ "${WK_PLATFORM_NAME}" =~ .*simulator ]]
then
    cp "${CODE_SIGN_ENTITLEMENTS}" "${WK_PROCESSED_XCENT_FILE}"
elif [[ "${WK_PLATFORM_NAME}" == macosx ]]
then
    [[ "${RC_XBS}" != YES ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then mac_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then mac_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then mac_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Plugin.64 ]]; then mac_process_plugin_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.GPU ]]; then mac_process_gpu_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebAuthn ]]; then mac_process_webauthn_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
elif [[ "${WK_PLATFORM_NAME}" == maccatalyst || "${WK_PLATFORM_NAME}" == iosmac ]]
then
    [[ "${RC_XBS}" != YES && ( "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development || "${PRODUCT_NAME}" == com.apple.WebKit.Plugin.64 ) ]] && plistbuddy Add :com.apple.security.get-task-allow bool YES

    if [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent.Development ]]; then maccatalyst_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebContent ]]; then maccatalyst_process_webcontent_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Networking ]]; then maccatalyst_process_network_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.Plugin.64 ]]; then maccatalyst_process_plugin_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.GPU ]]; then maccatalyst_process_gpu_entitlements
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
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.GPU ]]; then ios_family_process_gpu_entitlements
    elif [[ "${PRODUCT_NAME}" == com.apple.WebKit.WebAuthn ]]; then ios_family_process_webauthn_entitlements
    else echo "Unsupported/unknown product: ${PRODUCT_NAME}"
    fi
else
    echo "Unsupported/unknown platform: ${WK_PLATFORM_NAME}"
fi

exit 0
