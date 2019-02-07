#!/bin/bash

[[ ${WK_PLATFORM_NAME} == macosx ]] || exit 0

function plistbuddy()
{
    /usr/libexec/PlistBuddy -c "$*" "${WK_PROCESSED_XCENT_FILE}"
}

function process_webcontent_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit bool YES

    if [[ ${WK_USE_RESTRICTED_ENTITLEMENTS} == YES ]]
    then
        plistbuddy Add :com.apple.rootless.storage.WebKitWebContentSandbox bool YES
    fi

    process_webcontent_or_plugin_entitlements
}

function process_network_entitlements()
{
    if [[ ${WK_USE_RESTRICTED_ENTITLEMENTS} == YES ]]
    then
        if (( ${TARGET_MAC_OS_X_VERSION_MAJOR} >= 101500 ))
        then
            plistbuddy Add :com.apple.private.network.socket-delegate bool YES
        fi

        plistbuddy Add :com.apple.rootless.storage.WebKitNetworkingSandbox bool YES
    fi
}

function process_plugin_entitlements()
{
    plistbuddy Add :com.apple.security.cs.allow-jit                        bool YES
    plistbuddy Add :com.apple.security.cs.allow-unsigned-executable-memory bool YES
    plistbuddy Add :com.apple.security.cs.disable-library-validation       bool YES
    plistbuddy Add :com.apple.security.files.user-selected.read-write      bool YES
    plistbuddy Add :com.apple.security.print                               bool YES

    process_webcontent_or_plugin_entitlements
}

function process_webcontent_or_plugin_entitlements()
{
    if [[ ${WK_USE_RESTRICTED_ENTITLEMENTS} == YES ]]
    then
        if (( ${TARGET_MAC_OS_X_VERSION_MAJOR} >= 101400 ))
        then
            plistbuddy Add :com.apple.tcc.delegated-services array
            plistbuddy Add :com.apple.tcc.delegated-services:0 string kTCCServiceCamera
            plistbuddy Add :com.apple.tcc.delegated-services:1 string kTCCServiceMicrophone
        fi

        if [[ ${WK_WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT} == YES ]]
        then
            plistbuddy Add :com.apple.private.xpc.domain-extension bool YES
        fi
    fi

    if [[ ${WK_XPC_SERVICE_VARIANT} == Development ]]
    then
        plistbuddy Add :com.apple.security.cs.disable-library-validation bool YES
    fi
}

rm -f "${WK_PROCESSED_XCENT_FILE}"
[[ ${RC_XBS} == "YES" ]] || plistbuddy Add :com.apple.security.get-task-allow bool YES

[[ ${PRODUCT_NAME} =~ com.apple.WebKit.WebContent(.Development)? ]] && process_webcontent_entitlements
[[ ${PRODUCT_NAME} == com.apple.WebKit.Networking ]] && process_network_entitlements
[[ ${PRODUCT_NAME} == com.apple.WebKit.Plugin.64 ]] && process_plugin_entitlements

exit 0
