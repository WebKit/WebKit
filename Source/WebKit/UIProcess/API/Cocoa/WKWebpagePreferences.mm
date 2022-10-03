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
#import <WebKit/WKWebpagePreferences.h>

#import "APICustomHeaderFields.h"
#import "CaptivePortalModeObserver.h"
#import "WKUserContentControllerInternal.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebContentMode.h"
#import "WebProcessPool.h"
#import "_WKCustomHeaderFieldsInternal.h"
#import <WebCore/DocumentLoader.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import <wtf/cocoa/Entitlements.h>
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)

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

#endif // PLATFORM(IOS_FAMILY)

static _WKWebsiteMouseEventPolicy mouseEventPolicy(WebCore::MouseEventPolicy policy)
{
    switch (policy) {
    case WebCore::MouseEventPolicy::Default:
        return _WKWebsiteMouseEventPolicyDefault;
#if ENABLE(IOS_TOUCH_EVENTS)
    case WebCore::MouseEventPolicy::SynthesizeTouchEvents:
        return _WKWebsiteMouseEventPolicySynthesizeTouchEvents;
#endif
    }
    ASSERT_NOT_REACHED();
    return _WKWebsiteMouseEventPolicyDefault;
}

static WebCore::MouseEventPolicy coreMouseEventPolicy(_WKWebsiteMouseEventPolicy policy)
{
    switch (policy) {
    case _WKWebsiteMouseEventPolicyDefault:
        return WebCore::MouseEventPolicy::Default;
#if ENABLE(IOS_TOUCH_EVENTS)
    case _WKWebsiteMouseEventPolicySynthesizeTouchEvents:
        return WebCore::MouseEventPolicy::SynthesizeTouchEvents;
#endif
    }
    ASSERT_NOT_REACHED();
    return WebCore::MouseEventPolicy::Default;
}

static _WKWebsiteModalContainerObservationPolicy modalContainerObservationPolicy(WebCore::ModalContainerObservationPolicy policy)
{
    switch (policy) {
    case WebCore::ModalContainerObservationPolicy::Disabled:
        return _WKWebsiteModalContainerObservationPolicyDisabled;
    case WebCore::ModalContainerObservationPolicy::Prompt:
        return _WKWebsiteModalContainerObservationPolicyPrompt;
    }
    ASSERT_NOT_REACHED();
    return _WKWebsiteModalContainerObservationPolicyDisabled;
}

static WebCore::ModalContainerObservationPolicy coreModalContainerObservationPolicy(_WKWebsiteModalContainerObservationPolicy policy)
{
    switch (policy) {
    case _WKWebsiteModalContainerObservationPolicyDisabled:
        return WebCore::ModalContainerObservationPolicy::Disabled;
    case _WKWebsiteModalContainerObservationPolicyPrompt:
        return WebCore::ModalContainerObservationPolicy::Prompt;
    }
    ASSERT_NOT_REACHED();
    return WebCore::ModalContainerObservationPolicy::Disabled;
}

class WebPagePreferencesCaptivePortalModeObserver final : public CaptivePortalModeObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebPagePreferencesCaptivePortalModeObserver(id object)
        : m_object(object)
    {
        addCaptivePortalModeObserver(*this);
    }

    ~WebPagePreferencesCaptivePortalModeObserver()
    {
        removeCaptivePortalModeObserver(*this);
    }

private:
    void willChangeCaptivePortalMode() final
    {
        if (auto object = m_object.get()) {
            [object willChangeValueForKey:@"_captivePortalModeEnabled"];
            [object willChangeValueForKey:@"lockdownModeEnabled"];
        }
    }

    void didChangeCaptivePortalMode() final
    {
        if (auto object = m_object.get()) {
            [object didChangeValueForKey:@"_captivePortalModeEnabled"];
            [object didChangeValueForKey:@"lockdownModeEnabled"];
        }
    }

    WeakObjCPtr<id> m_object;
};

} // namespace WebKit

@implementation WKWebpagePreferences

+ (instancetype)defaultPreferences
{
    return adoptNS([[self alloc] init]).autorelease();
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWebpagePreferences.class, self))
        return;

    _websitePolicies->API::WebsitePolicies::~WebsitePolicies();

    [super dealloc];
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::WebsitePolicies>(self);
    _captivePortalModeObserver = makeUnique<WebKit::WebPagePreferencesCaptivePortalModeObserver>(self);

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

- (void)_setActiveContentRuleListActionPatterns:(NSDictionary<NSString *, NSSet<NSString *> *> *)patterns
{
    __block HashMap<String, Vector<String>> map;
    [patterns enumerateKeysAndObjectsUsingBlock:^(NSString *key, NSSet<NSString *> *value, BOOL *) {
        Vector<String> vector;
        vector.reserveInitialCapacity(value.count);
        for (NSString *pattern in value)
            vector.uncheckedAppend(pattern);
        map.add(key, WTFMove(vector));
    }];
    _websitePolicies->setActiveContentRuleListActionPatterns(WTFMove(map));
}

- (NSDictionary<NSString *, NSSet<NSString *> *> *)_activeContentRuleListActionPatterns
{
    NSMutableDictionary<NSString *, NSSet<NSString *> *> *dictionary = [NSMutableDictionary dictionary];
    for (const auto& pair : _websitePolicies->activeContentRuleListActionPatterns()) {
        NSMutableSet<NSString *> *set = [NSMutableSet set];
        for (const auto& pattern : pair.value)
            [set addObject:pattern];
        [dictionary setObject:set forKey:pair.key];
    }
    return dictionary;
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
    return createNSArray(_websitePolicies->customHeaderFields(), [] (auto& field) {
        return wrapper(API::CustomHeaderFields::create(field));
    }).autorelease();
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

- (WKUserContentController *)_userContentController
{
    return wrapper(_websitePolicies->userContentController());
}

- (void)_setUserContentController:(WKUserContentController *)userContentController
{
    _websitePolicies->setUserContentController(userContentController->_userContentControllerProxy.get());
}

- (void)_setCustomUserAgent:(NSString *)customUserAgent
{
    _websitePolicies->setCustomUserAgent(customUserAgent);
}

- (NSString *)_customUserAgent
{
    return _websitePolicies->customUserAgent();
}

- (void)_setCustomUserAgentAsSiteSpecificQuirks:(NSString *)customUserAgent
{
    _websitePolicies->setCustomUserAgentAsSiteSpecificQuirks(customUserAgent);
}

- (NSString *)_customUserAgentAsSiteSpecificQuirks
{
    return _websitePolicies->customUserAgentAsSiteSpecificQuirks();
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

- (void)setAllowsContentJavaScript:(BOOL)allowsContentJavaScript
{
    _websitePolicies->setAllowsContentJavaScript(allowsContentJavaScript ? WebCore::AllowsContentJavaScript::Yes : WebCore::AllowsContentJavaScript::No);
}

- (BOOL)allowsContentJavaScript
{
    switch (_websitePolicies->allowsContentJavaScript()) {
    case WebCore::AllowsContentJavaScript::Yes:
        return YES;
    case WebCore::AllowsContentJavaScript::No:
        return NO;
    }
}

- (void)_setCaptivePortalModeEnabled:(BOOL)captivePortalModeEnabled
{
#if PLATFORM(IOS_FAMILY)
    // On iOS, the web browser entitlement is required to disable captive portal mode.
    if (!captivePortalModeEnabled && !WTF::processHasEntitlement("com.apple.developer.web-browser"_s))
        [NSException raise:NSInternalInconsistencyException format:@"The 'com.apple.developer.web-browser' restricted entitlement is required to disable captive portal mode"];
#endif

    _websitePolicies->setCaptivePortalModeEnabled(!!captivePortalModeEnabled);
}

- (BOOL)_captivePortalModeEnabled
{
    return _websitePolicies->captivePortalModeEnabled();
}

- (void)_setAllowPrivacyProxy:(BOOL)allow
{
    _websitePolicies->setAllowPrivacyProxy(allow);
}

- (BOOL)_allowPrivacyProxy
{
    return _websitePolicies->allowPrivacyProxy();
}

- (_WKWebsiteColorSchemePreference)_colorSchemePreference
{
    switch (_websitePolicies->colorSchemePreference()) {
    case WebCore::ColorSchemePreference::NoPreference:
        return _WKWebsiteColorSchemePreferenceNoPreference;
    case WebCore::ColorSchemePreference::Light:
        return _WKWebsiteColorSchemePreferenceLight;
    case WebCore::ColorSchemePreference::Dark:
        return _WKWebsiteColorSchemePreferenceDark;
    }
}

- (void)_setColorSchemePreference:(_WKWebsiteColorSchemePreference)value
{
    switch (value) {
    case _WKWebsiteColorSchemePreferenceNoPreference:
        _websitePolicies->setColorSchemePreference(WebCore::ColorSchemePreference::NoPreference);
        break;
    case _WKWebsiteColorSchemePreferenceLight:
        _websitePolicies->setColorSchemePreference(WebCore::ColorSchemePreference::Light);
        break;
    case _WKWebsiteColorSchemePreferenceDark:
        _websitePolicies->setColorSchemePreference(WebCore::ColorSchemePreference::Dark);
        break;
    }
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

- (void)_setMouseEventPolicy:(_WKWebsiteMouseEventPolicy)policy
{
    _websitePolicies->setMouseEventPolicy(WebKit::coreMouseEventPolicy(policy));
}

- (_WKWebsiteMouseEventPolicy)_mouseEventPolicy
{
    return WebKit::mouseEventPolicy(_websitePolicies->mouseEventPolicy());
}

- (void)_setModalContainerObservationPolicy:(_WKWebsiteModalContainerObservationPolicy)policy
{
    _websitePolicies->setModalContainerObservationPolicy(WebKit::coreModalContainerObservationPolicy(policy));
}

- (_WKWebsiteModalContainerObservationPolicy)_modalContainerObservationPolicy
{
    return WebKit::modalContainerObservationPolicy(_websitePolicies->modalContainerObservationPolicy());
}

- (BOOL)isLockdownModeEnabled
{
#if ENABLE(LOCKDOWN_MODE_API)
    return _websitePolicies->captivePortalModeEnabled();
#else
    return NO;
#endif
}

- (void)setLockdownModeEnabled:(BOOL)lockdownModeEnabled
{
#if ENABLE(LOCKDOWN_MODE_API)
#if PLATFORM(IOS_FAMILY)
    // On iOS, the web browser entitlement is required to disable lockdown mode.
    if (!lockdownModeEnabled && !WTF::processHasEntitlement("com.apple.developer.web-browser"_s))
        [NSException raise:NSInternalInconsistencyException format:@"The 'com.apple.developer.web-browser' restricted entitlement is required to disable lockdown mode"];
#endif

    _websitePolicies->setCaptivePortalModeEnabled(!!lockdownModeEnabled);
#endif
}

- (BOOL)_networkConnectionIntegrityEnabled
{
    return _websitePolicies->networkConnectionIntegrityEnabled();
}

- (void)_setNetworkConnectionIntegrityEnabled:(BOOL)networkConnectionIntegrityEnabled
{
    _websitePolicies->setNetworkConnectionIntegrityEnabled(networkConnectionIntegrityEnabled);
}

@end
