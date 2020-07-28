/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DefaultWebBrowserChecks.h"

#import "TCCSPI.h"
#import "VersionChecks.h"
#import <WebCore/RegistrableDomain.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <wtf/HashMap.h>
#import <wtf/RunLoop.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/StringHash.h>

SOFT_LINK_PRIVATE_FRAMEWORK(TCC)
SOFT_LINK(TCC, TCCAccessPreflight, TCCAccessPreflightResult, (CFStringRef service, CFDictionaryRef options), (service, options))
SOFT_LINK(TCC, TCCAccessPreflightWithAuditToken, TCCAccessPreflightResult, (CFStringRef service, audit_token_t token, CFDictionaryRef options), (service, token, options))
SOFT_LINK_CONSTANT(TCC, kTCCServiceWebKitIntelligentTrackingPrevention, CFStringRef)


namespace WebKit {

static bool isFullWebBrowser(const String&);

bool shouldBeTreatedAsFullBrowser(const String& bundleID)
{
    return bundleID == "com.apple.WebKit.TestWebKitAPI"_s || bundleID == "com.apple.WebKit.WebKitTestRunner"_s;
}

Optional<Vector<WebCore::RegistrableDomain>> getAppBoundDomainsTesting(const String& bundleID)
{
    if (bundleID.isNull())
        return WTF::nullopt;

    static auto appBoundDomainList = makeNeverDestroyed(HashMap<String, Vector<WebCore::RegistrableDomain>> {
        {"inAppBrowserPrivacyTestIdentifier"_s, Vector<WebCore::RegistrableDomain> { WebCore::RegistrableDomain::uncheckedCreateFromRegistrableDomainString("127.0.0.1") }},
    });

    auto appBoundDomainIter = appBoundDomainList->find(bundleID);
    if (appBoundDomainIter != appBoundDomainList->end())
        return appBoundDomainIter->value;

    return WTF::nullopt;
}

#if ASSERT_ENABLED
static bool isInWebKitChildProcess()
{
    static bool isInSubProcess;

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
        isInSubProcess = [bundleIdentifier hasPrefix:@"com.apple.WebKit.WebContent"]
            || [bundleIdentifier hasPrefix:@"com.apple.WebKit.Networking"]
            || [bundleIdentifier hasPrefix:@"com.apple.WebKit.GPU"];
    });

    return isInSubProcess;
}
#endif

enum class ITPState : uint8_t {
    Uninitialized,
    Initializing,
    Enabled,
    Disabled
};

static std::atomic<ITPState> g_currentITPState = ITPState::Uninitialized;

static bool hasRequestedCrossWebsiteTrackingPermission()
{
    ASSERT(!isInWebKitChildProcess());

    static std::atomic<bool> hasRequestedCrossWebsiteTrackingPermission = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSCrossWebsiteTrackingUsageDescription"];
    return hasRequestedCrossWebsiteTrackingPermission;
}

static bool determineITPStateInternal(bool appWasLinkedOnOrAfter, const String& bundleIdentifier)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(!isInWebKitChildProcess());

    if (!appWasLinkedOnOrAfter)
        return false;

    if (!isFullWebBrowser(bundleIdentifier) && !hasRequestedCrossWebsiteTrackingPermission())
        return true;

    TCCAccessPreflightResult result = kTCCAccessPreflightDenied;
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000)
    result = TCCAccessPreflight(getkTCCServiceWebKitIntelligentTrackingPrevention(), nullptr);
#endif
    return result != kTCCAccessPreflightDenied;
}

static dispatch_queue_t g_itpQueue;

void determineITPState()
{
    ASSERT(RunLoop::isMain());
    if (g_currentITPState != ITPState::Uninitialized)
        return;

    g_currentITPState = ITPState::Initializing;
    bool appWasLinkedOnOrAfter = linkedOnOrAfter(SDKVersion::FirstWithSessionCleanupByDefault, AssumeSafariIsAlwaysLinkedOnAfter::Yes);

    g_itpQueue = dispatch_queue_create("com.apple.WebKit.itpCheckQueue", NULL);

    dispatch_async(g_itpQueue, [appWasLinkedOnOrAfter, bundleIdentifier = WebCore::applicationBundleIdentifier().isolatedCopy()] {
        g_currentITPState = determineITPStateInternal(appWasLinkedOnOrAfter, bundleIdentifier) ? ITPState::Enabled : ITPState::Disabled;

        dispatch_async(dispatch_get_main_queue(), ^{
            dispatch_release(g_itpQueue);
            g_itpQueue = nullptr;
        });
    });
}

bool doesAppHaveITPEnabled()
{
    ASSERT(!isInWebKitChildProcess());
    ASSERT(RunLoop::isMain());
    if (g_currentITPState > ITPState::Initializing)
        return g_currentITPState == ITPState::Enabled;

    RELEASE_ASSERT(g_itpQueue);

    __block bool isITPEnabled;
    dispatch_sync(g_itpQueue, ^{
        isITPEnabled = g_currentITPState == ITPState::Enabled;
    });
    return isITPEnabled;
}

bool doesParentProcessHaveITPEnabled(Optional<audit_token_t> auditToken)
{
    ASSERT(isInWebKitChildProcess());
    ASSERT(RunLoop::isMain());

    if (!isParentProcessAFullWebBrowser(auditToken))
        return true;

    TCCAccessPreflightResult result = kTCCAccessPreflightDenied;
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000)
    if (auditToken)
        result = TCCAccessPreflightWithAuditToken(getkTCCServiceWebKitIntelligentTrackingPrevention(), auditToken.value(), nullptr);
#endif
    return result != kTCCAccessPreflightDenied;
}

static std::atomic<bool> hasCheckedUsageStrings = false;
bool hasProhibitedUsageStrings()
{
    ASSERT(!isInWebKitChildProcess());

    static bool hasProhibitedUsageStrings = false;

    if (hasCheckedUsageStrings)
        return hasProhibitedUsageStrings;

    NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];
    RELEASE_ASSERT(infoDictionary);

    // See <rdar://problem/59979468> for details about how this list was selected.
    auto prohibitedStrings = @[
        @"NSHomeKitUsageDescription",
        @"NSBluetoothAlwaysUsageDescription",
        @"NSPhotoLibraryUsageDescription",
        @"NSHealthShareUsageDescription",
        @"NSHealthUpdateUsageDescription",
        @"NSLocationAlwaysUsageDescription",
        @"NSLocationAlwaysAndWhenInUseUsageDescription"
    ];

    for (NSString *prohibitedString : prohibitedStrings) {
        if ([infoDictionary objectForKey:prohibitedString]) {
            String message = [NSString stringWithFormat:@"[In-App Browser Privacy] %@ used prohibited usage string %@.", [[NSBundle mainBundle] bundleIdentifier], prohibitedString];
            WTFLogAlways(message.utf8().data());
            hasProhibitedUsageStrings = true;
            break;
        }
    }
    hasCheckedUsageStrings = true;
    return hasProhibitedUsageStrings;
}

bool isParentProcessAFullWebBrowser(Optional<audit_token_t> auditToken)
{
    ASSERT(isInWebKitChildProcess());
    RELEASE_ASSERT(auditToken);

    static bool fullWebBrowser;

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        fullWebBrowser = WTF::hasEntitlement(auditToken.value(), "com.apple.developer.web-browser");
    });

    return fullWebBrowser || shouldBeTreatedAsFullBrowser(WebCore::applicationBundleIdentifier());
}

static bool isFullWebBrowser(const String& bundleIdentifier)
{
    ASSERT(!isInWebKitChildProcess());

    static bool fullWebBrowser = WTF::processHasEntitlement("com.apple.developer.web-browser");

    return fullWebBrowser || shouldBeTreatedAsFullBrowser(bundleIdentifier);
}

bool isFullWebBrowser()
{
    ASSERT(!isInWebKitChildProcess());
    ASSERT(RunLoop::isMain());

    return isFullWebBrowser(WebCore::applicationBundleIdentifier());
}

} // namespace WebKit
