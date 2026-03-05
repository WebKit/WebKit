#!/bin/bash
set -e

function plistbuddy()
{
    /usr/libexec/PlistBuddy -c "$*" "${WK_PROCESSED_XCENT_FILE}"
}

function plistbuddy_base()
{
    if [[ "${WK_PLATFORM_NAME}" =~ .*simulator ]]
    then
        /usr/libexec/PlistBuddy -c "$*" "${WK_PROCESSED_SIMULATOR_XCENT_FILE}"
    else
        plistbuddy "$*"
    fi
}

# ========================================
# Common entitlements
# ========================================

process_base_testwebkitapi_entitlements()
{
    if [[ "${CONFIGURATION}" != Production ]]
    then
        plistbuddy_base Add :com.apple.security.get-task-allow bool YES
    fi
}

process_restricted_testwebkitapi_entitlements()
{
    plistbuddy Add :keychain-access-groups array
    plistbuddy Add :keychain-access-groups:0 string com.apple.TestWebKitAPIAlternate
    plistbuddy Add :keychain-access-groups:1 string com.apple.TestWebKitAPI
    plistbuddy Add :com.apple.private.webkit.adattributiond.testing bool YES
    plistbuddy Add :com.apple.private.webkit.webpush bool YES
    plistbuddy Add :com.apple.private.webkit.webpush.inject bool YES
    plistbuddy Add :com.apple.private.messages.enhanced-link-security bool YES
}

# ========================================
# Mac entitlements
# ========================================

function process_mac_testwebkitapi_entitlements()
{
    process_base_testwebkitapi_entitlements

    plistbuddy Add :com.apple.security.temporary-exception.sbpl array
    plistbuddy Add :com.apple.security.temporary-exception.sbpl:0 string '(allow mach-issue-extension (require-all (extension-class \"com.apple.webkit.extension.mach\")))'
    plistbuddy Add :com.apple.security.temporary-exception.sbpl:1 string '(allow iokit-issue-extension (require-all (extension-class \"com.apple.webkit.extension.iokit\")))'

    if [[ "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
    then
        process_restricted_testwebkitapi_entitlements

        plistbuddy Add :com.apple.hid.manager.user-access-device bool YES
        plistbuddy Add :com.apple.private.hid.client.event-filter bool YES
    fi
}

# ========================================
# iOS Family entitlements
# ========================================

function process_ios_family_testwebkitapi_entitlements()
{
    process_base_testwebkitapi_entitlements
    process_restricted_testwebkitapi_entitlements

    plistbuddy Add :com.apple.developer.WebKit.ServiceWorkers bool YES
    plistbuddy Add :com.apple.Pasteboard.paste-unchecked bool YES
    plistbuddy Add :com.apple.private.xpc.launchd.job-manager string TestWebKitAPI
    plistbuddy Add :com.apple.CommCenter.fine-grained array
    plistbuddy Add :com.apple.CommCenter.fine-grained:0 string public-cellular-plan

    if [[ "${PRODUCT_BUNDLE_IDENTIFIER}" == org.webkit.TestWebKitAPI ]]
    then
        plistbuddy Add :com.apple.developer.web-browser bool YES
        plistbuddy Add :com.apple.developer.web-browser-engine.host bool YES
    fi
}

plistbuddy Clear dict
plistbuddy_base Clear dict

if [[ "${WK_PLATFORM_NAME}" == macosx || "${WK_PLATFORM_NAME}" == maccatalyst ]]
then
    process_mac_testwebkitapi_entitlements
elif [[ "${WK_IS_COCOA_TOUCH}" == YES ]]
then
    process_ios_family_testwebkitapi_entitlements
else
    echo "Unsupported/unknown platform: ${WK_PLATFORM_NAME}"
    exit 1
fi

exit 0
