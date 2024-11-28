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
#import "LockdownModeObserver.h"
#import "WKUserContentControllerInternal.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebContentMode.h"
#import "WebProcessPool.h"
#import "_WKCustomHeaderFieldsInternal.h"
#import <WebCore/DocumentLoader.h>
#import <WebCore/ElementTargetingTypes.h>
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

WKWebpagePreferencesUpgradeToHTTPSPolicy upgradeToHTTPSPolicy(WebCore::HTTPSByDefaultMode httpsByDefault)
{
    switch (httpsByDefault) {
    case WebCore::HTTPSByDefaultMode::Disabled:
        return WKWebpagePreferencesUpgradeToHTTPSPolicyKeepAsRequested;
    case WebCore::HTTPSByDefaultMode::UpgradeWithAutomaticFallback:
        return WKWebpagePreferencesUpgradeToHTTPSPolicyAutomaticFallbackToHTTP;
    case WebCore::HTTPSByDefaultMode::UpgradeWithUserMediatedFallback:
        return WKWebpagePreferencesUpgradeToHTTPSPolicyUserMediatedFallbackToHTTP;
    case WebCore::HTTPSByDefaultMode::UpgradeAndNoFallback:
        return WKWebpagePreferencesUpgradeToHTTPSPolicyErrorOnFailure;
    }
    ASSERT_NOT_REACHED();
    return WKWebpagePreferencesUpgradeToHTTPSPolicyKeepAsRequested;
}

WebCore::HTTPSByDefaultMode httpsByDefaultMode(WKWebpagePreferencesUpgradeToHTTPSPolicy upgradeToHTTPSPolicy)
{
    switch (upgradeToHTTPSPolicy) {
    case WKWebpagePreferencesUpgradeToHTTPSPolicyKeepAsRequested:
        return WebCore::HTTPSByDefaultMode::Disabled;
    case WKWebpagePreferencesUpgradeToHTTPSPolicyAutomaticFallbackToHTTP:
        return WebCore::HTTPSByDefaultMode::UpgradeWithAutomaticFallback;
    case WKWebpagePreferencesUpgradeToHTTPSPolicyUserMediatedFallbackToHTTP:
        return WebCore::HTTPSByDefaultMode::UpgradeWithUserMediatedFallback;
    case WKWebpagePreferencesUpgradeToHTTPSPolicyErrorOnFailure:
        return WebCore::HTTPSByDefaultMode::UpgradeAndNoFallback;
    }

    ASSERT_NOT_REACHED();
    return WebCore::HTTPSByDefaultMode::Disabled;
}

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

} // namespace WebKit

@implementation WKWebpagePreferences

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

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

    return self;
}

- (void)_setContentBlockersEnabled:(BOOL)contentBlockersEnabled
{
    auto defaultEnablement = contentBlockersEnabled ? WebCore::ContentExtensionDefaultEnablement::Enabled : WebCore::ContentExtensionDefaultEnablement::Disabled;
    _websitePolicies->setContentExtensionEnablement({ defaultEnablement, { } });
}

- (BOOL)_contentBlockersEnabled
{
    // Note that this only reports default state, and ignores exceptions. This should be turned into a no-op and
    // eventually removed, once no more internal clients rely on it.
    return _websitePolicies->contentExtensionEnablement().first == WebCore::ContentExtensionDefaultEnablement::Enabled;
}

- (void)_setContentRuleListsEnabled:(BOOL)enabled exceptions:(NSSet<NSString *> *)identifiers
{
    HashSet<String> exceptions;
    exceptions.reserveInitialCapacity(identifiers.count);
    for (NSString *identifier in identifiers)
        exceptions.add(identifier);

    auto defaultEnablement = enabled ? WebCore::ContentExtensionDefaultEnablement::Enabled : WebCore::ContentExtensionDefaultEnablement::Disabled;
    _websitePolicies->setContentExtensionEnablement({ defaultEnablement, WTFMove(exceptions) });
}

- (void)_setActiveContentRuleListActionPatterns:(NSDictionary<NSString *, NSSet<NSString *> *> *)patterns
{
    __block HashMap<String, Vector<String>> map;
    [patterns enumerateKeysAndObjectsUsingBlock:^(NSString *key, NSSet<NSString *> *value, BOOL *) {
        Vector<String> vector;
        vector.reserveInitialCapacity(value.count);
        for (NSString *pattern in value)
            vector.append(pattern);
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
    Vector<WebCore::CustomHeaderFields> vector(fields.count, [fields](size_t i) {
        _WKCustomHeaderFields *element = fields[i];
        return static_cast<API::CustomHeaderFields&>([element _apiObject]).coreFields();
    });
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

- (void)_setCaptivePortalModeEnabled:(BOOL)enabled
{
#if PLATFORM(IOS_FAMILY)
    // On iOS, the web browser entitlement is required to disable Lockdown mode.
    if (!enabled && !WTF::processHasEntitlement("com.apple.developer.web-browser"_s) && !WTF::processHasEntitlement("com.apple.private.allow-ldm-exempt-webview"_s))
        [NSException raise:NSInternalInconsistencyException format:@"The 'com.apple.developer.web-browser' restricted entitlement is required to disable Lockdown mode"];
#endif

    _websitePolicies->setLockdownModeEnabled(!!enabled);
}

- (BOOL)_captivePortalModeEnabled
{
    return _websitePolicies->lockdownModeEnabled();
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
    return _websitePolicies->lockdownModeEnabled();
#else
    return NO;
#endif
}

- (void)setLockdownModeEnabled:(BOOL)lockdownModeEnabled
{
#if ENABLE(LOCKDOWN_MODE_API)
#if PLATFORM(IOS_FAMILY)
    // On iOS, the web browser entitlement is required to disable lockdown mode.
    if (!lockdownModeEnabled && !WTF::processHasEntitlement("com.apple.developer.web-browser"_s) && !WTF::processHasEntitlement("com.apple.private.allow-ldm-exempt-webview"_s))
        [NSException raise:NSInternalInconsistencyException format:@"The 'com.apple.developer.web-browser' restricted entitlement is required to disable lockdown mode"];
#endif

    _websitePolicies->setLockdownModeEnabled(!!lockdownModeEnabled);
#endif
}

- (void)setPreferredHTTPSNavigationPolicy:(WKWebpagePreferencesUpgradeToHTTPSPolicy)upgradeToHTTPSPolicy
{
    _websitePolicies->setHTTPSByDefault(WebKit::httpsByDefaultMode(upgradeToHTTPSPolicy));
}

- (WKWebpagePreferencesUpgradeToHTTPSPolicy)preferredHTTPSNavigationPolicy
{
    return WebKit::upgradeToHTTPSPolicy(_websitePolicies->httpsByDefaultMode());
}

- (BOOL)_networkConnectionIntegrityEnabled
{
    return _websitePolicies->advancedPrivacyProtections().containsAll({
        WebCore::AdvancedPrivacyProtections::BaselineProtections,
        WebCore::AdvancedPrivacyProtections::FingerprintingProtections,
        WebCore::AdvancedPrivacyProtections::EnhancedNetworkPrivacy,
        WebCore::AdvancedPrivacyProtections::LinkDecorationFiltering,
    });
}

- (void)_setNetworkConnectionIntegrityEnabled:(BOOL)enabled
{
    auto webCorePolicy = _websitePolicies->advancedPrivacyProtections();
    webCorePolicy.set(WebCore::AdvancedPrivacyProtections::BaselineProtections, enabled);
    webCorePolicy.set(WebCore::AdvancedPrivacyProtections::FingerprintingProtections, enabled);
    webCorePolicy.set(WebCore::AdvancedPrivacyProtections::EnhancedNetworkPrivacy, enabled);
    webCorePolicy.set(WebCore::AdvancedPrivacyProtections::LinkDecorationFiltering, enabled);
    _websitePolicies->setAdvancedPrivacyProtections(webCorePolicy);
}

- (_WKWebsiteNetworkConnectionIntegrityPolicy)_networkConnectionIntegrityPolicy
{
    _WKWebsiteNetworkConnectionIntegrityPolicy policy = _WKWebsiteNetworkConnectionIntegrityPolicyNone;
    auto webCorePolicy = _websitePolicies->advancedPrivacyProtections();

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::BaselineProtections))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyEnabled;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::HTTPSFirst))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSFirst;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::HTTPSOnly))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::HTTPSOnlyExplicitlyBypassedForDomain))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnlyExplicitlyBypassedForDomain;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::FailClosed))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyFailClosed;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::WebSearchContent))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyWebSearchContent;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::FingerprintingProtections))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::EnhancedNetworkPrivacy))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicyRequestValidation;

    if (webCorePolicy.contains(WebCore::AdvancedPrivacyProtections::LinkDecorationFiltering))
        policy |= _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters;

    return policy;
}

- (void)_setNetworkConnectionIntegrityPolicy:(_WKWebsiteNetworkConnectionIntegrityPolicy)advancedPrivacyProtections
{
    OptionSet<WebCore::AdvancedPrivacyProtections> webCorePolicy;

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyEnabled)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::BaselineProtections);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSFirst)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::HTTPSFirst);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::HTTPSOnly);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnlyExplicitlyBypassedForDomain)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::HTTPSOnlyExplicitlyBypassedForDomain);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyFailClosed)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::FailClosed);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyWebSearchContent)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::WebSearchContent);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::FingerprintingProtections);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicyRequestValidation)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::EnhancedNetworkPrivacy);

    if (advancedPrivacyProtections & _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters)
        webCorePolicy.add(WebCore::AdvancedPrivacyProtections::LinkDecorationFiltering);

    _websitePolicies->setAdvancedPrivacyProtections(webCorePolicy);
}

- (void)_setVisibilityAdjustmentSelectorsIncludingShadowHosts:(NSArray<NSArray<NSSet<NSString *> *> *> *)elements
{
    Vector<WebCore::TargetedElementSelectors> result;
    result.reserveInitialCapacity(elements.count);
    for (NSArray<NSSet<NSString *> *> *nsSelectorsForElement in elements) {
        WebCore::TargetedElementSelectors selectorsForElement;
        selectorsForElement.reserveInitialCapacity(nsSelectorsForElement.count);
        for (NSSet<NSString *> *nsSelectors in nsSelectorsForElement) {
            HashSet<String> selectors;
            selectors.reserveInitialCapacity(nsSelectors.count);
            for (NSString *selector in nsSelectors)
                selectors.add(selector);
            selectorsForElement.append(WTFMove(selectors));
        }
        result.append(WTFMove(selectorsForElement));
    }
    _websitePolicies->setVisibilityAdjustmentSelectors(WTFMove(result));
}

- (NSArray<NSArray<NSSet<NSString *> *> *> *)_visibilityAdjustmentSelectorsIncludingShadowHosts
{
    RetainPtr result = adoptNS([[NSMutableArray alloc] initWithCapacity:_websitePolicies->visibilityAdjustmentSelectors().size()]);
    for (auto& selectorsForElement : _websitePolicies->visibilityAdjustmentSelectors()) {
        RetainPtr nsSelectorsForElement = adoptNS([[NSMutableArray alloc] initWithCapacity:selectorsForElement.size()]);
        for (auto& selectors : selectorsForElement) {
            RetainPtr nsSelectors = adoptNS([[NSMutableSet alloc] initWithCapacity:selectors.size()]);
            for (auto& selector : selectors)
                [nsSelectors addObject:selector];
            [nsSelectorsForElement addObject:nsSelectors.get()];
        }
        [result addObject:nsSelectorsForElement.get()];
    }
    return result.autorelease();
}

- (void)_setVisibilityAdjustmentSelectors:(NSSet<NSString *> *)nsSelectors
{
    RetainPtr elements = adoptNS([[NSMutableArray alloc] initWithCapacity:nsSelectors.count]);
    for (NSString *selector : nsSelectors)
        [elements addObject:@[ [NSSet setWithObject:selector] ]];
    self._visibilityAdjustmentSelectorsIncludingShadowHosts = elements.get();
}

- (NSSet<NSString *> *)_visibilityAdjustmentSelectors
{
    RetainPtr selectors = adoptNS([[NSMutableSet alloc] init]);
    for (auto& elementSelectors : _websitePolicies->visibilityAdjustmentSelectors()) {
        if (elementSelectors.size() != 1) {
            // Ignore shadow roots for compatibility with this soon-to-be deprecated method.
            continue;
        }

        for (auto& selector : elementSelectors.first())
            [selectors addObject:selector];
    }
    return selectors.autorelease();
}

- (BOOL)_pushAndNotificationAPIEnabled
{
    return _websitePolicies->pushAndNotificationsEnabledPolicy() == WebKit::WebsitePushAndNotificationsEnabledPolicy::Yes;
}

- (void)_setPushAndNotificationAPIEnabled:(BOOL)enabled
{
    _websitePolicies->setPushAndNotificationsEnabledPolicy(enabled ? WebKit::WebsitePushAndNotificationsEnabledPolicy::Yes : WebKit::WebsitePushAndNotificationsEnabledPolicy::No);
}

@end
