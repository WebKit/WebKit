/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WKWebpagePreferences.h"

#import "APICustomHeaderFields.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebContentMode.h"
#import "_WKCustomHeaderFieldsInternal.h"
#import "_WKWebsitePoliciesInternal.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

namespace WebKit {

WKContentMode contentMode(WebKit::WebContentMode contentMode)
{
    switch (contentMode) {
    case WebKit::WebContentMode::Recommended:
        return WKContentModeRecommended;
    case WebKit::WebContentMode::Mobile:
        return WKContentModeMobile;
    case WebKit::WebContentMode::Desktop:
        return WKContentModeDesktop;
    }
    ASSERT_NOT_REACHED();
    return WKContentModeRecommended;
}

WebKit::WebContentMode webContentMode(WKContentMode contentMode)
{
    switch (contentMode) {
    case WKContentModeRecommended:
        return WebKit::WebContentMode::Recommended;
    case WKContentModeMobile:
        return WebKit::WebContentMode::Mobile;
    case WKContentModeDesktop:
        return WebKit::WebContentMode::Desktop;
    }
    ASSERT_NOT_REACHED();
    return WebKit::WebContentMode::Recommended;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)

@implementation WKWebpagePreferences

+ (instancetype)defaultPreferences
{
    return [[[self alloc] init] autorelease];
}

- (void)dealloc
{
    _websitePolicies->API::WebsitePolicies::~WebsitePolicies();

    [super dealloc];
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::WebsitePolicies>(self);

    return self;
}

- (void)_setContentBlockersEnabled:(BOOL)contentBlockersEnabled
{
    _websitePolicies->setContentBlockersEnabled(contentBlockersEnabled);
}

- (BOOL)_contentBlockersEnabled
{
    return _websitePolicies->contentBlockersEnabled();
}

- (void)_setAllowedAutoplayQuirks:(_WKWebsiteAutoplayQuirk)allowedQuirks
{
    OptionSet<WebKit::WebsiteAutoplayQuirk> quirks;

    if (allowedQuirks & _WKWebsiteAutoplayQuirkInheritedUserGestures)
        quirks.add(WebKit::WebsiteAutoplayQuirk::InheritedUserGestures);

    if (allowedQuirks & _WKWebsiteAutoplayQuirkSynthesizedPauseEvents)
        quirks.add(WebKit::WebsiteAutoplayQuirk::SynthesizedPauseEvents);

    if (allowedQuirks & _WKWebsiteAutoplayQuirkArbitraryUserGestures)
        quirks.add(WebKit::WebsiteAutoplayQuirk::ArbitraryUserGestures);

    if (allowedQuirks & _WKWebsiteAutoplayQuirkPerDocumentAutoplayBehavior)
        quirks.add(WebKit::WebsiteAutoplayQuirk::PerDocumentAutoplayBehavior);

    _websitePolicies->setAllowedAutoplayQuirks(quirks);
}

- (_WKWebsiteAutoplayQuirk)_allowedAutoplayQuirks
{
    _WKWebsiteAutoplayQuirk quirks = 0;
    auto allowedQuirks = _websitePolicies->allowedAutoplayQuirks();

    if (allowedQuirks.contains(WebKit::WebsiteAutoplayQuirk::InheritedUserGestures))
        quirks |= _WKWebsiteAutoplayQuirkInheritedUserGestures;

    if (allowedQuirks.contains(WebKit::WebsiteAutoplayQuirk::SynthesizedPauseEvents))
        quirks |= _WKWebsiteAutoplayQuirkSynthesizedPauseEvents;

    if (allowedQuirks.contains(WebKit::WebsiteAutoplayQuirk::ArbitraryUserGestures))
        quirks |= _WKWebsiteAutoplayQuirkArbitraryUserGestures;

    if (allowedQuirks.contains(WebKit::WebsiteAutoplayQuirk::PerDocumentAutoplayBehavior))
        quirks |= _WKWebsiteAutoplayQuirkPerDocumentAutoplayBehavior;

    return quirks;
}

- (void)_setAutoplayPolicy:(_WKWebsiteAutoplayPolicy)policy
{
    switch (policy) {
    case _WKWebsiteAutoplayPolicyDefault:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::Default);
        break;
    case _WKWebsiteAutoplayPolicyAllow:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::Allow);
        break;
    case _WKWebsiteAutoplayPolicyAllowWithoutSound:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::AllowWithoutSound);
        break;
    case _WKWebsiteAutoplayPolicyDeny:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::Deny);
        break;
    }
}

- (_WKWebsiteAutoplayPolicy)_autoplayPolicy
{
    switch (_websitePolicies->autoplayPolicy()) {
    case WebKit::WebsiteAutoplayPolicy::Default:
        return _WKWebsiteAutoplayPolicyDefault;
    case WebKit::WebsiteAutoplayPolicy::Allow:
        return _WKWebsiteAutoplayPolicyAllow;
    case WebKit::WebsiteAutoplayPolicy::AllowWithoutSound:
        return _WKWebsiteAutoplayPolicyAllowWithoutSound;
    case WebKit::WebsiteAutoplayPolicy::Deny:
        return _WKWebsiteAutoplayPolicyDeny;
    }
}

#if ENABLE(DEVICE_ORIENTATION)
static WebCore::DeviceOrientationOrMotionPermissionState toDeviceOrientationOrMotionPermissionState(_WKWebsiteDeviceOrientationAndMotionAccessPolicy policy)
{
    switch (policy) {
    case _WKWebsiteDeviceOrientationAndMotionAccessPolicyAsk:
        return WebCore::DeviceOrientationOrMotionPermissionState::Prompt;
    case _WKWebsiteDeviceOrientationAndMotionAccessPolicyGrant:
        return WebCore::DeviceOrientationOrMotionPermissionState::Granted;
    case _WKWebsiteDeviceOrientationAndMotionAccessPolicyDeny:
        break;
    }
    return WebCore::DeviceOrientationOrMotionPermissionState::Denied;
}
#endif

- (void)_setDeviceOrientationAndMotionAccessPolicy:(_WKWebsiteDeviceOrientationAndMotionAccessPolicy)policy
{
#if ENABLE(DEVICE_ORIENTATION)
    _websitePolicies->setDeviceOrientationAndMotionAccessState(toDeviceOrientationOrMotionPermissionState(policy));
#endif
}

#if ENABLE(DEVICE_ORIENTATION)
static _WKWebsiteDeviceOrientationAndMotionAccessPolicy toWKWebsiteDeviceOrientationAndMotionAccessPolicy(WebCore::DeviceOrientationOrMotionPermissionState state)
{
    switch (state) {
    case WebCore::DeviceOrientationOrMotionPermissionState::Prompt:
        return _WKWebsiteDeviceOrientationAndMotionAccessPolicyAsk;
    case WebCore::DeviceOrientationOrMotionPermissionState::Granted:
        return _WKWebsiteDeviceOrientationAndMotionAccessPolicyGrant;
    case WebCore::DeviceOrientationOrMotionPermissionState::Denied:
        break;
    }
    return _WKWebsiteDeviceOrientationAndMotionAccessPolicyDeny;
}
#endif

- (_WKWebsiteDeviceOrientationAndMotionAccessPolicy)_deviceOrientationAndMotionAccessPolicy
{
#if ENABLE(DEVICE_ORIENTATION)
    return toWKWebsiteDeviceOrientationAndMotionAccessPolicy(_websitePolicies->deviceOrientationAndMotionAccessState());
#else
    return _WKWebsiteDeviceOrientationAndMotionAccessPolicyDeny;
#endif
}

- (void)_setPopUpPolicy:(_WKWebsitePopUpPolicy)policy
{
    switch (policy) {
    case _WKWebsitePopUpPolicyDefault:
        _websitePolicies->setPopUpPolicy(WebKit::WebsitePopUpPolicy::Default);
        break;
    case _WKWebsitePopUpPolicyAllow:
        _websitePolicies->setPopUpPolicy(WebKit::WebsitePopUpPolicy::Allow);
        break;
    case _WKWebsitePopUpPolicyBlock:
        _websitePolicies->setPopUpPolicy(WebKit::WebsitePopUpPolicy::Block);
        break;
    }
}

- (_WKWebsitePopUpPolicy)_popUpPolicy
{
    switch (_websitePolicies->popUpPolicy()) {
    case WebKit::WebsitePopUpPolicy::Default:
        return _WKWebsitePopUpPolicyDefault;
    case WebKit::WebsitePopUpPolicy::Allow:
        return _WKWebsitePopUpPolicyAllow;
    case WebKit::WebsitePopUpPolicy::Block:
        return _WKWebsitePopUpPolicyBlock;
    }
}

- (NSArray<_WKCustomHeaderFields *> *)_customHeaderFields
{
    const auto& fields = _websitePolicies->customHeaderFields();
    NSMutableArray *array = [[[NSMutableArray alloc] initWithCapacity:fields.size()] autorelease];
    for (const auto& field : fields)
        [array addObject:wrapper(API::CustomHeaderFields::create(field))];
    return array;
}

- (void)_setCustomHeaderFields:(NSArray<_WKCustomHeaderFields *> *)fields
{
    Vector<WebCore::CustomHeaderFields> vector;
    vector.reserveInitialCapacity(fields.count);
    for (_WKCustomHeaderFields *element in fields)
        vector.uncheckedAppend(static_cast<API::CustomHeaderFields&>([element _apiObject]).coreFields());
    _websitePolicies->setCustomHeaderFields(WTFMove(vector));
}

- (WKWebsiteDataStore *)_websiteDataStore
{
    return wrapper(_websitePolicies->websiteDataStore());
}

- (void)_setWebsiteDataStore:(WKWebsiteDataStore *)websiteDataStore
{
    _websitePolicies->setWebsiteDataStore(websiteDataStore->_websiteDataStore.get());
}

- (void)_setCustomUserAgent:(NSString *)customUserAgent
{
    _websitePolicies->setCustomUserAgent(customUserAgent);
}

- (NSString *)_customUserAgent
{
    return _websitePolicies->customUserAgent();
}

- (void)_setCustomJavaScriptUserAgentAsSiteSpecificQuirks:(NSString *)customUserAgent
{
    _websitePolicies->setCustomJavaScriptUserAgentAsSiteSpecificQuirks(customUserAgent);
}

- (NSString *)_customJavaScriptUserAgentAsSiteSpecificQuirks
{
    return _websitePolicies->customJavaScriptUserAgentAsSiteSpecificQuirks();
}

- (void)_setCustomNavigatorPlatform:(NSString *)customNavigatorPlatform
{
    _websitePolicies->setCustomNavigatorPlatform(customNavigatorPlatform);
}

- (NSString *)_customNavigatorPlatform
{
    return _websitePolicies->customNavigatorPlatform();
}

- (BOOL)_allowSiteSpecificQuirksToOverrideCompatibilityMode
{
    return _websitePolicies->allowSiteSpecificQuirksToOverrideContentMode();
}

- (void)_setAllowSiteSpecificQuirksToOverrideCompatibilityMode:(BOOL)value
{
    _websitePolicies->setAllowSiteSpecificQuirksToOverrideContentMode(value);
}

- (NSString *)_applicationNameForUserAgentWithModernCompatibility
{
    return _websitePolicies->applicationNameForDesktopUserAgent();
}

- (void)_setApplicationNameForUserAgentWithModernCompatibility:(NSString *)applicationName
{
    _websitePolicies->setApplicationNameForDesktopUserAgent(applicationName);
}

- (API::Object&)_apiObject
{
    return *_websitePolicies;
}

#if PLATFORM(IOS_FAMILY)

- (void)setPreferredContentMode:(WKContentMode)contentMode
{
    _websitePolicies->setPreferredContentMode(WebKit::webContentMode(contentMode));
}

- (WKContentMode)preferredContentMode
{
    return WebKit::contentMode(_websitePolicies->preferredContentMode());
}

#endif // PLATFORM(IOS_FAMILY)

@end
