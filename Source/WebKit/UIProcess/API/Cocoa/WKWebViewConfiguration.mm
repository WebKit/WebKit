/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "WKWebViewConfigurationInternal.h"

#import "APIPageConfiguration.h"
#import "CSPExtensionUtilities.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKUserContentControllerInternal.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebKit2Initialize.h"
#import "WebPreferencesDefaultValues.h"
#import "WebPreferencesDefinitions.h"
#import "WebURLSchemeHandlerCocoa.h"
#import "_WKApplicationManifestInternal.h"
#import "_WKVisitedLinkStoreInternal.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Settings.h>
#import <WebKit/WKProcessPool.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKUserContentController.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <wtf/RetainPtr.h>
#import <wtf/RobinHoodHashSet.h>
#import <wtf/URLParser.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#import "_WKWebExtensionControllerInternal.h"
#endif

#if PLATFORM(IOS_FAMILY)

_WKDragLiftDelay toDragLiftDelay(NSUInteger value)
{
    if (value == _WKDragLiftDelayMedium)
        return _WKDragLiftDelayMedium;
    if (value == _WKDragLiftDelayLong)
        return _WKDragLiftDelayLong;
    return _WKDragLiftDelayShort;
}

_WKDragLiftDelay toWKDragLiftDelay(WebKit::DragLiftDelay delay)
{
    if (delay == WebKit::DragLiftDelay::Medium)
        return _WKDragLiftDelayMedium;
    if (delay == WebKit::DragLiftDelay::Long)
        return _WKDragLiftDelayLong;
    return _WKDragLiftDelayShort;
}

WebKit::DragLiftDelay fromWKDragLiftDelay(_WKDragLiftDelay delay)
{
    if (delay == _WKDragLiftDelayMedium)
        return WebKit::DragLiftDelay::Medium;
    if (delay == _WKDragLiftDelayLong)
        return WebKit::DragLiftDelay::Long;
    return WebKit::DragLiftDelay::Short;
}

#endif // PLATFORM(IOS_FAMILY)

@implementation WKWebViewConfiguration {
    RefPtr<API::PageConfiguration> _pageConfiguration;
    WeakObjCPtr<WKWebView> _relatedWebView;
    WeakObjCPtr<WKWebView> _webViewToCloneSessionStorageFrom;
    WeakObjCPtr<WKWebView> _alternateWebViewForNavigationGestures;
    RetainPtr<NSString> _groupIdentifier;
    std::optional<RetainPtr<NSString>> _applicationNameForUserAgent;

    Class _attachmentFileWrapperClass;

    BOOL _controlledByAutomation;

#if ENABLE(APPLE_PAY)
    BOOL _applePayEnabled;
#endif
#if ENABLE(APP_HIGHLIGHTS)
    BOOL _appHighlightsEnabled;
#endif
    double _sampledPageTopColorMaxDifference;
    double _sampledPageTopColorMinHeight;

    RetainPtr<NSString> _mediaContentTypesRequiringHardwareSupport;
    RetainPtr<NSArray<NSString *>> _additionalSupportedImageTypes;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    WebKit::InitializeWebKit2();

    _pageConfiguration = API::PageConfiguration::create();

#if PLATFORM(IOS_FAMILY)
#if !PLATFORM(WATCHOS)
    _allowsPictureInPictureMediaPlayback = YES;
#endif

#if !PLATFORM(WATCHOS)
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::MediaTypesRequiringUserActionForPlayback))
        _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAudio;
    else
#endif
        _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAll;
    _ignoresViewportScaleLimits = NO;
#else
    _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    _userInterfaceDirectionPolicy = WKUserInterfaceDirectionPolicyContent;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    _allowsAirPlayForMediaPlayback = YES;
#endif

#if PLATFORM(IOS_FAMILY)
    _selectionGranularity = WKSelectionGranularityDynamic;
#endif // PLATFORM(IOS_FAMILY)

    _mediaContentTypesRequiringHardwareSupport = @"";

#if ENABLE(APPLE_PAY)
    _applePayEnabled = DEFAULT_VALUE_FOR_ApplePayEnabled;
#endif

#if ENABLE(APP_HIGHLIGHTS)
    _appHighlightsEnabled = DEFAULT_VALUE_FOR_AppHighlightsEnabled;
#endif

    _sampledPageTopColorMaxDifference = DEFAULT_VALUE_FOR_SampledPageTopColorMaxDifference;
    _sampledPageTopColorMinHeight = DEFAULT_VALUE_FOR_SampledPageTopColorMinHeight;

    return self;
}

- (void)setAllowsInlinePredictions:(BOOL)enabled
{
    _pageConfiguration->setAllowsInlinePredictions(enabled);
}

- (BOOL)allowsInlinePredictions
{
    return _pageConfiguration->allowsInlinePredictions();
}

#if PLATFORM(IOS_FAMILY)
- (void)setAllowsInlineMediaPlayback:(BOOL)allows
{
    _pageConfiguration->setAllowsInlineMediaPlayback(allows);
}

- (BOOL)allowsInlineMediaPlayback
{
    return _pageConfiguration->allowsInlineMediaPlayback();
}
#endif

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; processPool = %@; preferences = %@>", NSStringFromClass(self.class), self, self.processPool, self.preferences];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:self.processPool forKey:@"processPool"];
    [coder encodeObject:self.preferences forKey:@"preferences"];
    [coder encodeObject:self.userContentController forKey:@"userContentController"];
    [coder encodeObject:self.websiteDataStore forKey:@"websiteDataStore"];

    [coder encodeBool:self.suppressesIncrementalRendering forKey:@"suppressesIncrementalRendering"];

    if (_applicationNameForUserAgent)
        [coder encodeObject:self.applicationNameForUserAgent forKey:@"applicationNameForUserAgent"];

    [coder encodeBool:self.allowsAirPlayForMediaPlayback forKey:@"allowsAirPlayForMediaPlayback"];

    [coder encodeBool:self._drawsBackground forKey:@"drawsBackground"];

#if PLATFORM(IOS_FAMILY)
    [coder encodeInteger:self.dataDetectorTypes forKey:@"dataDetectorTypes"];
    [coder encodeBool:self.allowsInlineMediaPlayback forKey:@"allowsInlineMediaPlayback"];
    [coder encodeBool:self._allowsInlineMediaPlaybackAfterFullscreen forKey:@"allowsInlineMediaPlaybackAfterFullscreen"];
    [coder encodeBool:self.mediaTypesRequiringUserActionForPlayback forKey:@"mediaTypesRequiringUserActionForPlayback"];
    [coder encodeInteger:self.selectionGranularity forKey:@"selectionGranularity"];
    [coder encodeBool:self.allowsPictureInPictureMediaPlayback forKey:@"allowsPictureInPictureMediaPlayback"];
    [coder encodeBool:self.ignoresViewportScaleLimits forKey:@"ignoresViewportScaleLimits"];
    [coder encodeInteger:self._dragLiftDelay forKey:@"dragLiftDelay"];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [coder encodeBool:self._textInteractionGesturesEnabled forKey:@"textInteractionGesturesEnabled"];
ALLOW_DEPRECATED_DECLARATIONS_END
    [coder encodeBool:self._longPressActionsEnabled forKey:@"longPressActionsEnabled"];
    [coder encodeBool:self._systemPreviewEnabled forKey:@"systemPreviewEnabled"];
    [coder encodeBool:self._shouldDecidePolicyBeforeLoadingQuickLookPreview forKey:@"shouldDecidePolicyBeforeLoadingQuickLookPreview"];
#else
    [coder encodeInteger:self.userInterfaceDirectionPolicy forKey:@"userInterfaceDirectionPolicy"];
#endif
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    self.processPool = [coder decodeObjectOfClass:[WKProcessPool class] forKey:@"processPool"];
    self.preferences = [coder decodeObjectOfClass:[WKPreferences class] forKey:@"preferences"];
    self.userContentController = [coder decodeObjectOfClass:[WKUserContentController class] forKey:@"userContentController"];
    self.websiteDataStore = [coder decodeObjectOfClass:[WKWebsiteDataStore class] forKey:@"websiteDataStore"];

    self.suppressesIncrementalRendering = [coder decodeBoolForKey:@"suppressesIncrementalRendering"];

    if ([coder containsValueForKey:@"applicationNameForUserAgent"])
        self.applicationNameForUserAgent = [coder decodeObjectOfClass:[NSString class] forKey:@"applicationNameForUserAgent"];

    self.allowsAirPlayForMediaPlayback = [coder decodeBoolForKey:@"allowsAirPlayForMediaPlayback"];

    if ([coder containsValueForKey:@"drawsBackground"])
        self._drawsBackground = [coder decodeBoolForKey:@"drawsBackground"];

#if PLATFORM(IOS_FAMILY)
    self.dataDetectorTypes = [coder decodeIntegerForKey:@"dataDetectorTypes"];
    self.allowsInlineMediaPlayback = [coder decodeBoolForKey:@"allowsInlineMediaPlayback"];
    self._allowsInlineMediaPlaybackAfterFullscreen = [coder decodeBoolForKey:@"allowsInlineMediaPlaybackAfterFullscreen"];
    self.mediaTypesRequiringUserActionForPlayback = [coder decodeBoolForKey:@"mediaTypesRequiringUserActionForPlayback"];
    self.selectionGranularity = static_cast<WKSelectionGranularity>([coder decodeIntegerForKey:@"selectionGranularity"]);
    self.allowsPictureInPictureMediaPlayback = [coder decodeBoolForKey:@"allowsPictureInPictureMediaPlayback"];
    self.ignoresViewportScaleLimits = [coder decodeBoolForKey:@"ignoresViewportScaleLimits"];
    self._dragLiftDelay = toDragLiftDelay([coder decodeIntegerForKey:@"dragLiftDelay"]);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    self._textInteractionGesturesEnabled = [coder decodeBoolForKey:@"textInteractionGesturesEnabled"];
ALLOW_DEPRECATED_DECLARATIONS_END
    self._longPressActionsEnabled = [coder decodeBoolForKey:@"longPressActionsEnabled"];
    self._systemPreviewEnabled = [coder decodeBoolForKey:@"systemPreviewEnabled"];
    self._shouldDecidePolicyBeforeLoadingQuickLookPreview = [coder decodeBoolForKey:@"shouldDecidePolicyBeforeLoadingQuickLookPreview"];
#else
    auto userInterfaceDirectionPolicyCandidate = static_cast<WKUserInterfaceDirectionPolicy>([coder decodeIntegerForKey:@"userInterfaceDirectionPolicy"]);
    if (userInterfaceDirectionPolicyCandidate == WKUserInterfaceDirectionPolicyContent || userInterfaceDirectionPolicyCandidate == WKUserInterfaceDirectionPolicySystem)
        self.userInterfaceDirectionPolicy = userInterfaceDirectionPolicyCandidate;
#endif

    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    WKWebViewConfiguration *configuration = [(WKWebViewConfiguration *)[[self class] allocWithZone:zone] init];

    configuration->_pageConfiguration = _pageConfiguration->copy();
    configuration.processPool = self.processPool;
    configuration.preferences = self.preferences;
    configuration.userContentController = self.userContentController;
    if (self._websiteDataStoreIfExists)
        [configuration setWebsiteDataStore:self._websiteDataStoreIfExists];
    configuration.defaultWebpagePreferences = self.defaultWebpagePreferences;
    configuration._visitedLinkStore = self._visitedLinkStore;
    configuration._relatedWebView = _relatedWebView.get().get();
    configuration._webViewToCloneSessionStorageFrom = _webViewToCloneSessionStorageFrom.get().get();
    configuration._alternateWebViewForNavigationGestures = _alternateWebViewForNavigationGestures.get().get();

    configuration->_suppressesIncrementalRendering = self->_suppressesIncrementalRendering;
    configuration->_applicationNameForUserAgent = self->_applicationNameForUserAgent;

    configuration->_attachmentFileWrapperClass = self->_attachmentFileWrapperClass;
    configuration->_mediaTypesRequiringUserActionForPlayback = self->_mediaTypesRequiringUserActionForPlayback;
    configuration->_controlledByAutomation = self->_controlledByAutomation;

#if PLATFORM(IOS_FAMILY)
    configuration->_allowsPictureInPictureMediaPlayback = self->_allowsPictureInPictureMediaPlayback;
    configuration->_selectionGranularity = self->_selectionGranularity;
    configuration->_ignoresViewportScaleLimits = self->_ignoresViewportScaleLimits;
#endif
#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
    configuration->_dataDetectorTypes = self->_dataDetectorTypes;
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    configuration->_allowsAirPlayForMediaPlayback = self->_allowsAirPlayForMediaPlayback;
#endif
#if ENABLE(APPLE_PAY)
    configuration->_applePayEnabled = self->_applePayEnabled;
#endif

    configuration->_mediaContentTypesRequiringHardwareSupport = adoptNS([self._mediaContentTypesRequiringHardwareSupport copyWithZone:zone]);
    configuration->_additionalSupportedImageTypes = adoptNS([self->_additionalSupportedImageTypes copyWithZone:zone]);

    configuration->_groupIdentifier = adoptNS([self->_groupIdentifier copyWithZone:zone]);

#if ENABLE(APP_HIGHLIGHTS)
    configuration->_appHighlightsEnabled = self->_appHighlightsEnabled;
#endif

    configuration->_sampledPageTopColorMaxDifference = self->_sampledPageTopColorMaxDifference;
    configuration->_sampledPageTopColorMinHeight = self->_sampledPageTopColorMinHeight;

    return configuration;
}

- (WKProcessPool *)processPool
{
    return wrapper(_pageConfiguration->processPool());
}

- (void)setProcessPool:(WKProcessPool *)processPool
{
    _pageConfiguration->setProcessPool(processPool ? processPool->_processPool.get() : nullptr);
}

- (WKPreferences *)preferences
{
    return wrapper(_pageConfiguration->preferences());
}

- (void)setPreferences:(WKPreferences *)preferences
{
    _pageConfiguration->setPreferences(preferences ? preferences->_preferences.get() : nullptr);
}

- (WKUserContentController *)userContentController
{
    return wrapper(_pageConfiguration->userContentController());
}

- (void)setUserContentController:(WKUserContentController *)userContentController
{
    _pageConfiguration->setUserContentController(userContentController ? userContentController->_userContentControllerProxy.get() : nullptr);
}

- (NSURL *)_requiredWebExtensionBaseURL
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return _pageConfiguration->requiredWebExtensionBaseURL();
#else
    return nil;
#endif
}

- (void)_setRequiredWebExtensionBaseURL:(NSURL *)baseURL
{
#if ENABLE(WK_WEB_EXTENSIONS)
    _pageConfiguration->setRequiredWebExtensionBaseURL(baseURL);
#endif
}

- (_WKWebExtensionController *)_strongWebExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return wrapper(_pageConfiguration->webExtensionController());
#else
    return nil;
#endif
}

- (_WKWebExtensionController *)_webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return self._weakWebExtensionController ?: self._strongWebExtensionController;
#else
    return nil;
#endif
}

- (void)_setWebExtensionController:(_WKWebExtensionController *)webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    _pageConfiguration->setWebExtensionController(webExtensionController ? &webExtensionController._webExtensionController : nullptr);
#endif
}

- (_WKWebExtensionController *)_weakWebExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return wrapper(_pageConfiguration->weakWebExtensionController());
#else
    return nil;
#endif
}

- (void)_setWeakWebExtensionController:(_WKWebExtensionController *)webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    _pageConfiguration->setWeakWebExtensionController(webExtensionController ? &webExtensionController._webExtensionController : nullptr);
#endif
}

- (BOOL)upgradeKnownHostsToHTTPS
{
    return _pageConfiguration->httpsUpgradeEnabled();
}

- (void)setUpgradeKnownHostsToHTTPS:(BOOL)upgrade
{
    _pageConfiguration->setHTTPSUpgradeEnabled(upgrade);
}

- (WKWebsiteDataStore *)websiteDataStore
{
    return wrapper(_pageConfiguration->websiteDataStore());
}

- (void)setWebsiteDataStore:(WKWebsiteDataStore *)websiteDataStore
{
    _pageConfiguration->setWebsiteDataStore(websiteDataStore ? websiteDataStore->_websiteDataStore.get() : nullptr);
}

- (WKWebpagePreferences *)defaultWebpagePreferences
{
    return wrapper(_pageConfiguration->defaultWebsitePolicies());
}

- (void)setDefaultWebpagePreferences:(WKWebpagePreferences *)defaultWebpagePreferences
{
    _pageConfiguration->setDefaultWebsitePolicies(defaultWebpagePreferences ? defaultWebpagePreferences->_websitePolicies.get() : nullptr);
}

static NSString *defaultApplicationNameForUserAgent()
{
#if PLATFORM(IOS_FAMILY)
    return @"Mobile/15E148";
#else
    return nil;
#endif
}

- (NSString *)_applicationNameForDesktopUserAgent
{
    return _applicationNameForUserAgent.value_or(nil).get();
}

- (NSString *)applicationNameForUserAgent
{
    return _applicationNameForUserAgent.value_or(defaultApplicationNameForUserAgent()).get();
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationNameForUserAgent
{
    _applicationNameForUserAgent.emplace(adoptNS(applicationNameForUserAgent.copy));
}

- (_WKVisitedLinkStore *)_visitedLinkStore
{
    return wrapper(_pageConfiguration->visitedLinkStore());
}

- (void)_setVisitedLinkStore:(_WKVisitedLinkStore *)visitedLinkStore
{
    _pageConfiguration->setVisitedLinkStore(visitedLinkStore ? visitedLinkStore->_visitedLinkStore.get() : nullptr);
}

- (void)setURLSchemeHandler:(id <WKURLSchemeHandler>)urlSchemeHandler forURLScheme:(NSString *)urlScheme
{
    if ([WKWebView handlesURLScheme:urlScheme])
        [NSException raise:NSInvalidArgumentException format:@"'%@' is a URL scheme that WKWebView handles natively", urlScheme];

    auto canonicalScheme = WTF::URLParser::maybeCanonicalizeScheme(String(urlScheme));
    if (!canonicalScheme)
        [NSException raise:NSInvalidArgumentException format:@"'%@' is not a valid URL scheme", urlScheme];

    if (_pageConfiguration->urlSchemeHandlerForURLScheme(*canonicalScheme))
        [NSException raise:NSInvalidArgumentException format:@"URL scheme '%@' already has a registered URL scheme handler", urlScheme];

    _pageConfiguration->setURLSchemeHandlerForURLScheme(WebKit::WebURLSchemeHandlerCocoa::create(urlSchemeHandler), *canonicalScheme);
}

- (id <WKURLSchemeHandler>)urlSchemeHandlerForURLScheme:(NSString *)urlScheme
{
    auto canonicalScheme = WTF::URLParser::maybeCanonicalizeScheme(String(urlScheme));
    if (!canonicalScheme)
        return nil;

    auto handler = _pageConfiguration->urlSchemeHandlerForURLScheme(*canonicalScheme);
    if (!handler || !handler->isAPIHandler())
        return nil;

    return static_cast<WebKit::WebURLSchemeHandlerCocoa*>(handler.get())->apiHandler();
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)limitsNavigationsToAppBoundDomains
{
    return _pageConfiguration->limitsNavigationsToAppBoundDomains();
}

- (void)setLimitsNavigationsToAppBoundDomains:(BOOL)limitsToAppBoundDomains
{
    _pageConfiguration->setLimitsNavigationsToAppBoundDomains(limitsToAppBoundDomains);
}
#endif

- (Ref<API::PageConfiguration>)copyPageConfiguration
{
    return _pageConfiguration->copy();
}

@end

@implementation WKWebViewConfiguration (WKPrivate)

- (WKWebView *)_relatedWebView
{
    return _relatedWebView.getAutoreleased();
}

- (void)_setRelatedWebView:(WKWebView *)relatedWebView
{
    _relatedWebView = relatedWebView;
}

- (WKWebView *)_webViewToCloneSessionStorageFrom
{
    return _webViewToCloneSessionStorageFrom.getAutoreleased();
}

- (void)_setWebViewToCloneSessionStorageFrom:(WKWebView *)webViewToCloneSessionStorageFrom
{
    _webViewToCloneSessionStorageFrom = webViewToCloneSessionStorageFrom;
}

- (WKWebView *)_alternateWebViewForNavigationGestures
{
    return _alternateWebViewForNavigationGestures.getAutoreleased();
}

- (void)_setAlternateWebViewForNavigationGestures:(WKWebView *)alternateView
{
    _alternateWebViewForNavigationGestures = alternateView;
}

- (NSString *)_groupIdentifier
{
    return _groupIdentifier.get();
}

- (void)_setGroupIdentifier:(NSString *)groupIdentifier
{
    _groupIdentifier = groupIdentifier;
}

- (BOOL)_respectsImageOrientation
{
    return self.preferences->_preferences->shouldRespectImageOrientation();
}

- (void)_setRespectsImageOrientation:(BOOL)respectsImageOrientation
{
    self.preferences->_preferences->setShouldRespectImageOrientation(respectsImageOrientation);
}

- (BOOL)_printsBackgrounds
{
    return self.preferences.shouldPrintBackgrounds;
}

- (void)_setPrintsBackgrounds:(BOOL)printsBackgrounds
{
    self.preferences.shouldPrintBackgrounds = printsBackgrounds;
}

- (NSTimeInterval)_incrementalRenderingSuppressionTimeout
{
    return _pageConfiguration->incrementalRenderingSuppressionTimeout();
}

- (void)_setIncrementalRenderingSuppressionTimeout:(NSTimeInterval)incrementalRenderingSuppressionTimeout
{
    _pageConfiguration->setIncrementalRenderingSuppressionTimeout(incrementalRenderingSuppressionTimeout);
}

- (BOOL)_allowsJavaScriptMarkup
{
    return _pageConfiguration->allowsJavaScriptMarkup();
}

- (void)_setAllowsJavaScriptMarkup:(BOOL)allowsJavaScriptMarkup
{
    _pageConfiguration->setAllowsJavaScriptMarkup(allowsJavaScriptMarkup);
}

- (BOOL)_allowUniversalAccessFromFileURLs
{
    return _pageConfiguration->allowUniversalAccessFromFileURLs();
}

- (void)_setAllowUniversalAccessFromFileURLs:(BOOL)allowUniversalAccessFromFileURLs
{
    _pageConfiguration->setAllowUniversalAccessFromFileURLs(allowUniversalAccessFromFileURLs);
}

- (BOOL)_allowTopNavigationToDataURLs
{
    return _pageConfiguration->allowTopNavigationToDataURLs();
}

- (void)_setAllowTopNavigationToDataURLs:(BOOL)allowTopNavigationToDataURLs
{
    _pageConfiguration->setAllowTopNavigationToDataURLs(allowTopNavigationToDataURLs);
}

- (BOOL)_convertsPositionStyleOnCopy
{
    return _pageConfiguration->convertsPositionStyleOnCopy();
}

- (void)_setConvertsPositionStyleOnCopy:(BOOL)convertsPositionStyleOnCopy
{
    _pageConfiguration->setConvertsPositionStyleOnCopy(convertsPositionStyleOnCopy);
}

- (BOOL)_allowsMetaRefresh
{
    return _pageConfiguration->allowsMetaRefresh();
}

- (void)_setAllowsMetaRefresh:(BOOL)allowsMetaRefresh
{
    _pageConfiguration->setAllowsMetaRefresh(allowsMetaRefresh);
}

- (BOOL)_clientNavigationsRunAtForegroundPriority
{
    return _pageConfiguration->clientNavigationsRunAtForegroundPriority();
}

- (void)_setClientNavigationsRunAtForegroundPriority:(BOOL)clientNavigationsRunAtForegroundPriority
{
    _pageConfiguration->setClientNavigationsRunAtForegroundPriority(clientNavigationsRunAtForegroundPriority);
}

- (NSArray<NSNumber *> *)_portsForUpgradingInsecureSchemeForTesting
{
    auto ports = _pageConfiguration->portsForUpgradingInsecureSchemeForTesting();
    if (ports)
        return @[@(ports->first), @(ports->second)];
    return nil;
}

- (void)_setPortsForUpgradingInsecureSchemeForTesting:(NSArray<NSNumber *> *)ports
{
    if (ports.count != 2 || ports[0].unsignedIntegerValue > std::numeric_limits<uint16_t>::max() || ports[1].unsignedIntegerValue > std::numeric_limits<uint16_t>::max())
        return;
    _pageConfiguration->setPortsForUpgradingInsecureSchemeForTesting((uint16_t)ports[0].unsignedIntegerValue, (uint16_t)ports[1].unsignedIntegerValue);
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)_alwaysRunsAtForegroundPriority
{
    return _pageConfiguration->clientNavigationsRunAtForegroundPriority();
}

- (void)_setAlwaysRunsAtForegroundPriority:(BOOL)alwaysRunsAtForegroundPriority
{
    _pageConfiguration->setClientNavigationsRunAtForegroundPriority(alwaysRunsAtForegroundPriority);
}

- (BOOL)_inlineMediaPlaybackRequiresPlaysInlineAttribute
{
    return _pageConfiguration->inlineMediaPlaybackRequiresPlaysInlineAttribute();
}

- (void)_setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)requiresPlaysInlineAttribute
{
    _pageConfiguration->setInlineMediaPlaybackRequiresPlaysInlineAttribute(requiresPlaysInlineAttribute);
}

- (BOOL)_allowsInlineMediaPlaybackAfterFullscreen
{
    return _pageConfiguration->allowsInlineMediaPlaybackAfterFullscreen();
}

- (void)_setAllowsInlineMediaPlaybackAfterFullscreen:(BOOL)allows
{
    _pageConfiguration->setAllowsInlineMediaPlaybackAfterFullscreen(allows);
}

- (_WKDragLiftDelay)_dragLiftDelay
{
    return toWKDragLiftDelay(_pageConfiguration->dragLiftDelay());
}

- (void)_setDragLiftDelay:(_WKDragLiftDelay)dragLiftDelay
{
    _pageConfiguration->setDragLiftDelay(fromWKDragLiftDelay(dragLiftDelay));
}

- (BOOL)_longPressActionsEnabled
{
    return _pageConfiguration->longPressActionsEnabled();
}

- (void)_setLongPressActionsEnabled:(BOOL)enabled
{
    _pageConfiguration->setLongPressActionsEnabled(enabled);
}

- (BOOL)_systemPreviewEnabled
{
    return _pageConfiguration->systemPreviewEnabled();
}

- (void)_setSystemPreviewEnabled:(BOOL)enabled
{
    _pageConfiguration->setSystemPreviewEnabled(enabled);
}

- (BOOL)_shouldDecidePolicyBeforeLoadingQuickLookPreview
{
    return _pageConfiguration->shouldDecidePolicyBeforeLoadingQuickLookPreview();
}

- (void)_setShouldDecidePolicyBeforeLoadingQuickLookPreview:(BOOL)shouldDecide
{
    _pageConfiguration->setShouldDecidePolicyBeforeLoadingQuickLookPreview(shouldDecide);
}

- (void)_setCanShowWhileLocked:(BOOL)value
{
    _pageConfiguration->setCanShowWhileLocked(value);
}

- (BOOL)_canShowWhileLocked
{
    return _pageConfiguration->canShowWhileLocked();
}

- (void)_setClickInteractionDriverForTesting:(id<_UIClickInteractionDriving>)driver
{
    _pageConfiguration->setClickInteractionDriverForTesting((NSObject<_UIClickInteractionDriving> *)driver);
}

- (id <_UIClickInteractionDriving>)_clickInteractionDriverForTesting
{
    return _pageConfiguration->clickInteractionDriverForTesting().get();
}

static _WKAttributionOverrideTesting toWKAttributionOverrideTesting(WebKit::AttributionOverrideTesting value)
{
    if (value == WebKit::AttributionOverrideTesting::AppInitiated)
        return _WKAttributionOverrideTestingAppInitiated;
    if (value == WebKit::AttributionOverrideTesting::UserInitiated)
        return _WKAttributionOverrideTestingUserInitiated;
    return _WKAttributionOverrideTestingNoOverride;
}

static WebKit::AttributionOverrideTesting toAttributionOverrideTesting(_WKAttributionOverrideTesting value)
{
    if (value == _WKAttributionOverrideTestingAppInitiated)
        return WebKit::AttributionOverrideTesting::AppInitiated;
    if (value == _WKAttributionOverrideTestingUserInitiated)
        return WebKit::AttributionOverrideTesting::UserInitiated;
    return WebKit::AttributionOverrideTesting::NoOverride;
}

- (void)_setAppInitiatedOverrideValueForTesting:(_WKAttributionOverrideTesting)value
{
    _pageConfiguration->setAppInitiatedOverrideValueForTesting(toAttributionOverrideTesting(value));
}

- (_WKAttributionOverrideTesting)_appInitiatedOverrideValueForTesting
{
    return toWKAttributionOverrideTesting(_pageConfiguration->appInitiatedOverrideValueForTesting());
}

#endif // PLATFORM(IOS_FAMILY)

- (BOOL)_ignoresAppBoundDomains
{
#if PLATFORM(IOS_FAMILY)
    return _pageConfiguration->ignoresAppBoundDomains();
#else
    return NO;
#endif
}

- (void)_setIgnoresAppBoundDomains:(BOOL)ignoresAppBoundDomains
{
#if PLATFORM(IOS_FAMILY)
    _pageConfiguration->setIgnoresAppBoundDomains(ignoresAppBoundDomains);
#endif
}

- (BOOL)_invisibleAutoplayNotPermitted
{
    return _pageConfiguration->invisibleAutoplayForbidden();
}

- (void)_setInvisibleAutoplayNotPermitted:(BOOL)notPermitted
{
    _pageConfiguration->setInvisibleAutoplayForbidden(notPermitted);
}

- (BOOL)_mediaDataLoadsAutomatically
{
    return _pageConfiguration->mediaDataLoadsAutomatically();
}

- (void)_setMediaDataLoadsAutomatically:(BOOL)mediaDataLoadsAutomatically
{
    _pageConfiguration->setMediaDataLoadsAutomatically(mediaDataLoadsAutomatically);
}

- (BOOL)_attachmentElementEnabled
{
    return _pageConfiguration->attachmentElementEnabled();
}

- (void)_setAttachmentElementEnabled:(BOOL)attachmentElementEnabled
{
    _pageConfiguration->setAttachmentElementEnabled(attachmentElementEnabled);
}

- (BOOL)_attachmentWideLayoutEnabled
{
    return _pageConfiguration->attachmentWideLayoutEnabled();
}

- (void)_setAttachmentWideLayoutEnabled:(BOOL)attachmentWideLayoutEnabled
{
    _pageConfiguration->setAttachmentWideLayoutEnabled(attachmentWideLayoutEnabled);
}

- (Class)_attachmentFileWrapperClass
{
    return _attachmentFileWrapperClass;
}

- (void)_setAttachmentFileWrapperClass:(Class)attachmentFileWrapperClass
{
    if (attachmentFileWrapperClass && ![attachmentFileWrapperClass isSubclassOfClass:[NSFileWrapper class]])
        [NSException raise:NSInvalidArgumentException format:@"Class %@ does not inherit from NSFileWrapper", attachmentFileWrapperClass];

    _attachmentFileWrapperClass = attachmentFileWrapperClass;
}

- (BOOL)_colorFilterEnabled
{
    return _pageConfiguration->colorFilterEnabled();
}

- (void)_setColorFilterEnabled:(BOOL)colorFilterEnabled
{
    _pageConfiguration->setColorFilterEnabled(colorFilterEnabled);
}

- (BOOL)_incompleteImageBorderEnabled
{
    return _pageConfiguration->incompleteImageBorderEnabled();
}

- (void)_setIncompleteImageBorderEnabled:(BOOL)incompleteImageBorderEnabled
{
    _pageConfiguration->setIncompleteImageBorderEnabled(incompleteImageBorderEnabled);
}

- (BOOL)_shouldDeferAsynchronousScriptsUntilAfterDocumentLoad
{
    return _pageConfiguration->shouldDeferAsynchronousScriptsUntilAfterDocumentLoad();
}

- (void)_setShouldDeferAsynchronousScriptsUntilAfterDocumentLoad:(BOOL)shouldDeferAsynchronousScriptsUntilAfterDocumentLoad
{
    _pageConfiguration->setShouldDeferAsynchronousScriptsUntilAfterDocumentLoad(shouldDeferAsynchronousScriptsUntilAfterDocumentLoad);
}

- (WKWebsiteDataStore *)_websiteDataStoreIfExists
{
    return wrapper(_pageConfiguration->websiteDataStoreIfExists());
}

- (NSArray<NSString *> *)_corsDisablingPatterns
{
    return createNSArray(_pageConfiguration->corsDisablingPatterns()).autorelease();
}

- (void)_setCORSDisablingPatterns:(NSArray<NSString *> *)patterns
{
    _pageConfiguration->setCORSDisablingPatterns(makeVector<String>(patterns));
}

- (NSSet<NSString *> *)_maskedURLSchemes
{
    const auto& schemes = _pageConfiguration->maskedURLSchemes();
    NSMutableSet<NSString *> *set = [NSMutableSet setWithCapacity:schemes.size()];
    for (const auto& scheme : schemes)
        [set addObject:scheme];
    return set;
}

- (void)_setMaskedURLSchemes:(NSSet<NSString *> *)schemes
{
    HashSet<String> set;
    for (NSString *scheme in schemes)
        set.add(scheme);
    _pageConfiguration->setMaskedURLSchemes(WTFMove(set));
}

- (void)_setLoadsFromNetwork:(BOOL)loads
{
    _pageConfiguration->setAllowedNetworkHosts(loads ? std::nullopt : std::optional { MemoryCompactLookupOnlyRobinHoodHashSet<String> { } });
}

- (BOOL)_loadsFromNetwork
{
    return _pageConfiguration->allowedNetworkHosts() == std::nullopt;
}

- (void)_setAllowedNetworkHosts:(NSSet<NSString *> *)hosts
{
    if (!hosts)
        return _pageConfiguration->setAllowedNetworkHosts(std::nullopt);
    MemoryCompactLookupOnlyRobinHoodHashSet<String> set;
    for (NSString *host in hosts)
        set.add(host);
    _pageConfiguration->setAllowedNetworkHosts(WTFMove(set));
}

- (NSSet<NSString *> *)_allowedNetworkHosts
{
    const auto& hosts = _pageConfiguration->allowedNetworkHosts();
    if (!hosts)
        return nil;
    NSMutableSet<NSString *> *set = [NSMutableSet setWithCapacity:hosts->size()];
    for (const auto& host : *hosts)
        [set addObject:host];
    return set;
}

- (void)_setLoadsSubresources:(BOOL)loads
{
    _pageConfiguration->setLoadsSubresources(loads);
}

- (BOOL)_loadsSubresources
{
    return _pageConfiguration->loadsSubresources();
}

- (BOOL)_deferrableUserScriptsShouldWaitUntilNotification
{
    return _pageConfiguration->userScriptsShouldWaitUntilNotification();
}

- (void)_setDeferrableUserScriptsShouldWaitUntilNotification:(BOOL)value
{
    _pageConfiguration->setUserScriptsShouldWaitUntilNotification(value);
}

- (void)_setCrossOriginAccessControlCheckEnabled:(BOOL)enabled
{
    _pageConfiguration->setCrossOriginAccessControlCheckEnabled(enabled);
}

- (BOOL)_crossOriginAccessControlCheckEnabled
{
    return _pageConfiguration->crossOriginAccessControlCheckEnabled();
}

- (BOOL)_drawsBackground
{
    return _pageConfiguration->drawsBackground();
}

- (void)_setDrawsBackground:(BOOL)drawsBackground
{
    _pageConfiguration->setDrawsBackground(drawsBackground);
}

- (BOOL)_requiresUserActionForVideoPlayback
{
    return self.mediaTypesRequiringUserActionForPlayback & WKAudiovisualMediaTypeVideo;
}

- (void)_setRequiresUserActionForVideoPlayback:(BOOL)requiresUserActionForVideoPlayback
{
    if (requiresUserActionForVideoPlayback)
        self.mediaTypesRequiringUserActionForPlayback |= WKAudiovisualMediaTypeVideo;
    else
        self.mediaTypesRequiringUserActionForPlayback &= ~WKAudiovisualMediaTypeVideo;
}

- (BOOL)_requiresUserActionForAudioPlayback
{
    return self.mediaTypesRequiringUserActionForPlayback & WKAudiovisualMediaTypeAudio;
}

- (void)_setRequiresUserActionForAudioPlayback:(BOOL)requiresUserActionForAudioPlayback
{
    if (requiresUserActionForAudioPlayback)
        self.mediaTypesRequiringUserActionForPlayback |= WKAudiovisualMediaTypeAudio;
    else
        self.mediaTypesRequiringUserActionForPlayback &= ~WKAudiovisualMediaTypeAudio;
}

- (BOOL)_mainContentUserGestureOverrideEnabled
{
    return _pageConfiguration->mainContentUserGestureOverrideEnabled();
}

- (void)_setMainContentUserGestureOverrideEnabled:(BOOL)mainContentUserGestureOverrideEnabled
{
    _pageConfiguration->setMainContentUserGestureOverrideEnabled(mainContentUserGestureOverrideEnabled);
}

- (BOOL)_initialCapitalizationEnabled
{
    return _pageConfiguration->initialCapitalizationEnabled();
}

- (void)_setInitialCapitalizationEnabled:(BOOL)initialCapitalizationEnabled
{
    _pageConfiguration->setInitialCapitalizationEnabled(initialCapitalizationEnabled);
}

- (BOOL)_waitsForPaintAfterViewDidMoveToWindow
{
    return _pageConfiguration->waitsForPaintAfterViewDidMoveToWindow();
}

- (void)_setWaitsForPaintAfterViewDidMoveToWindow:(BOOL)shouldSynchronize
{
    _pageConfiguration->setWaitsForPaintAfterViewDidMoveToWindow(shouldSynchronize);
}

- (BOOL)_isControlledByAutomation
{
    return _controlledByAutomation;
}

- (void)_setControlledByAutomation:(BOOL)controlledByAutomation
{
    _controlledByAutomation = controlledByAutomation;
}

- (_WKApplicationManifest *)_applicationManifest
{
    return wrapper(_pageConfiguration->applicationManifest());
}

- (void)_setApplicationManifest:(_WKApplicationManifest *)applicationManifest
{
    _pageConfiguration->setApplicationManifest(applicationManifest ? applicationManifest->_applicationManifest.get() : nullptr);
}

#if PLATFORM(MAC)
- (BOOL)_showsURLsInToolTips
{
    return _pageConfiguration->showsURLsInToolTips();
}

- (void)_setShowsURLsInToolTips:(BOOL)showsURLsInToolTips
{
    _pageConfiguration->setShowsURLsInToolTips(showsURLsInToolTips);
}

- (BOOL)_serviceControlsEnabled
{
    return _pageConfiguration->serviceControlsEnabled();
}

- (void)_setServiceControlsEnabled:(BOOL)serviceControlsEnabled
{
    _pageConfiguration->setServiceControlsEnabled(serviceControlsEnabled);
}

- (BOOL)_imageControlsEnabled
{
    return _pageConfiguration->imageControlsEnabled();
}

- (void)_setImageControlsEnabled:(BOOL)imageControlsEnabled
{
    _pageConfiguration->setImageControlsEnabled(imageControlsEnabled);
}

- (BOOL)_contextMenuQRCodeDetectionEnabled
{
    return _pageConfiguration->contextMenuQRCodeDetectionEnabled();
}

- (void)_setContextMenuQRCodeDetectionEnabled:(BOOL)contextMenuQRCodeDetectionEnabled
{
    _pageConfiguration->setContextMenuQRCodeDetectionEnabled(contextMenuQRCodeDetectionEnabled);
}

- (BOOL)_requiresUserActionForEditingControlsManager
{
    return _pageConfiguration->requiresUserActionForEditingControlsManager();
}

- (void)_setRequiresUserActionForEditingControlsManager:(BOOL)requiresUserAction
{
    _pageConfiguration->setRequiresUserActionForEditingControlsManager(requiresUserAction);
}

- (WKPageGroupRef)_pageGroup
{
    return nullptr;
}

- (void)_setPageGroup:(WKPageGroupRef)pageGroup
{
}

- (void)_setCPULimit:(double)cpuLimit
{
    _pageConfiguration->setCPULimit(cpuLimit);
}

- (double)_cpuLimit
{
    return _pageConfiguration->cpuLimit().value_or(0);
}

#endif // PLATFORM(MAC)

- (BOOL)_applePayEnabled
{
#if ENABLE(APPLE_PAY)
    return _applePayEnabled;
#else
    return NO;
#endif
}

- (void)_setApplePayEnabled:(BOOL)applePayEnabled
{
#if ENABLE(APPLE_PAY)
    _applePayEnabled = applePayEnabled;
#endif
}

- (BOOL)_needsStorageAccessFromFileURLsQuirk
{
    return _pageConfiguration->needsStorageAccessFromFileURLsQuirk();
}

- (void)_setNeedsStorageAccessFromFileURLsQuirk:(BOOL)needsLocalStorageQuirk
{
    _pageConfiguration->setNeedsStorageAccessFromFileURLsQuirk(needsLocalStorageQuirk);
}

- (NSString *)_overrideContentSecurityPolicy
{
    return _pageConfiguration->overrideContentSecurityPolicy();
}

- (void)_setOverrideContentSecurityPolicy:(NSString *)overrideContentSecurityPolicy
{
    _pageConfiguration->setOverrideContentSecurityPolicy(overrideContentSecurityPolicy);
}

- (NSString *)_mediaContentTypesRequiringHardwareSupport
{
    return _mediaContentTypesRequiringHardwareSupport.get();
}

- (void)_setMediaContentTypesRequiringHardwareSupport:(NSString *)mediaContentTypesRequiringHardwareSupport
{
    _mediaContentTypesRequiringHardwareSupport = adoptNS([mediaContentTypesRequiringHardwareSupport copy]);
}

- (NSArray<NSString *> *)_additionalSupportedImageTypes
{
    return _additionalSupportedImageTypes.get();
}

- (void)_setAdditionalSupportedImageTypes:(NSArray<NSString *> *)additionalSupportedImageTypes
{
    _additionalSupportedImageTypes = adoptNS([additionalSupportedImageTypes copy]);
}

- (void)_setLegacyEncryptedMediaAPIEnabled:(BOOL)enabled
{
    _pageConfiguration->setLegacyEncryptedMediaAPIEnabled(enabled);
}

- (BOOL)_legacyEncryptedMediaAPIEnabled
{
    return _pageConfiguration->legacyEncryptedMediaAPIEnabled();
}

- (void)_setAllowMediaContentTypesRequiringHardwareSupportAsFallback:(BOOL)allow
{
    _pageConfiguration->setAllowMediaContentTypesRequiringHardwareSupportAsFallback(allow);
}

- (BOOL)_allowMediaContentTypesRequiringHardwareSupportAsFallback
{
    return _pageConfiguration->allowMediaContentTypesRequiringHardwareSupportAsFallback();
}

- (BOOL)_mediaCaptureEnabled
{
    return _pageConfiguration->mediaCaptureEnabled();
}

- (void)_setMediaCaptureEnabled:(BOOL)enabled
{
    _pageConfiguration->setMediaCaptureEnabled(enabled);
}

- (void)_setUndoManagerAPIEnabled:(BOOL)enabled
{
    _pageConfiguration->setUndoManagerAPIEnabled(enabled);
}

- (BOOL)_undoManagerAPIEnabled
{
    return _pageConfiguration->undoManagerAPIEnabled();
}

- (void)_setAppHighlightsEnabled:(BOOL)enabled
{
#if ENABLE(APP_HIGHLIGHTS)
    _appHighlightsEnabled = enabled;
#endif
}

- (BOOL)_appHighlightsEnabled
{
#if ENABLE(APP_HIGHLIGHTS)
    return _appHighlightsEnabled;
#else
    return NO;
#endif
}

- (BOOL)_allowTestOnlyIPC
{
    return _pageConfiguration->allowTestOnlyIPC();
}

- (void)_setAllowTestOnlyIPC:(BOOL)allowTestOnlyIPC
{
    _pageConfiguration->setAllowTestOnlyIPC(allowTestOnlyIPC);
}

- (BOOL)_delaysWebProcessLaunchUntilFirstLoad
{
    return _pageConfiguration->delaysWebProcessLaunchUntilFirstLoad();
}

- (void)_setDelaysWebProcessLaunchUntilFirstLoad:(BOOL)delaysWebProcessLaunchUntilFirstLoad
{
    _pageConfiguration->setDelaysWebProcessLaunchUntilFirstLoad(delaysWebProcessLaunchUntilFirstLoad);
}

- (BOOL)_shouldRelaxThirdPartyCookieBlocking
{
    return _pageConfiguration->shouldRelaxThirdPartyCookieBlocking() == WebCore::ShouldRelaxThirdPartyCookieBlocking::Yes;
}

- (void)_setShouldRelaxThirdPartyCookieBlocking:(BOOL)relax
{
    bool allowed = WebCore::applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s;
#if PLATFORM(MAC)
    allowed |= WebCore::MacApplication::isSafari();
#elif PLATFORM(IOS_FAMILY)
    allowed |= WebCore::IOSApplication::isMobileSafari() || WebCore::IOSApplication::isSafariViewService();
#endif
#if ENABLE(WK_WEB_EXTENSIONS)
    allowed |= _pageConfiguration->requiredWebExtensionBaseURL().isValid();
#endif

    if (!allowed)
        [NSException raise:NSObjectNotAvailableException format:@"_shouldRelaxThirdPartyCookieBlocking may only be used by Safari."];

    _pageConfiguration->setShouldRelaxThirdPartyCookieBlocking(relax ? WebCore::ShouldRelaxThirdPartyCookieBlocking::Yes : WebCore::ShouldRelaxThirdPartyCookieBlocking::No);
}

- (NSString *)_processDisplayName
{
    return _pageConfiguration->processDisplayName();
}

- (void)_setProcessDisplayName:(NSString *)lsDisplayName
{
    _pageConfiguration->setProcessDisplayName(lsDisplayName);
}

- (void)_setSampledPageTopColorMaxDifference:(double)value
{
    _sampledPageTopColorMaxDifference = value;
}

- (double)_sampledPageTopColorMaxDifference
{
    return _sampledPageTopColorMaxDifference;
}

- (void)_setSampledPageTopColorMinHeight:(double)value
{
    _sampledPageTopColorMinHeight = value;
}

- (double)_sampledPageTopColorMinHeight
{
    return _sampledPageTopColorMinHeight;
}

- (void)_setAttributedBundleIdentifier:(NSString *)identifier
{
    _pageConfiguration->setAttributedBundleIdentifier(identifier);
}

- (NSString *)_attributedBundleIdentifier
{
    auto& identifier = _pageConfiguration->attributedBundleIdentifier();
    if (!identifier)
        return nil;
    return identifier;
}

- (void)_setContentSecurityPolicyModeForExtension:(_WKContentSecurityPolicyModeForExtension)mode
{
    _pageConfiguration->setContentSecurityPolicyModeForExtension(WebKit::toContentSecurityPolicyModeForExtension(mode));
}

- (_WKContentSecurityPolicyModeForExtension)_contentSecurityPolicyModeForExtension
{
    return WebKit::toWKContentSecurityPolicyModeForExtension(_pageConfiguration->contentSecurityPolicyModeForExtension());
}

// FIXME: Remove this SPI once rdar://110277838 is resolved and all clients adopt the API.
- (void)_setMarkedTextInputEnabled:(BOOL)enabled
{
    _pageConfiguration->setAllowsInlinePredictions(enabled);
}

- (BOOL)_markedTextInputEnabled
{
    return _pageConfiguration->allowsInlinePredictions();
}

@end

@implementation WKWebViewConfiguration (WKDeprecated)

#if PLATFORM(IOS_FAMILY)
- (BOOL)mediaPlaybackAllowsAirPlay
{
    return self.allowsAirPlayForMediaPlayback;
}

- (void)setMediaPlaybackAllowsAirPlay:(BOOL)allowed
{
    self.allowsAirPlayForMediaPlayback = allowed;
}

- (BOOL)mediaPlaybackRequiresUserAction
{
    return self.requiresUserActionForMediaPlayback;
}

- (void)setMediaPlaybackRequiresUserAction:(BOOL)required
{
    self.requiresUserActionForMediaPlayback = required;
}

- (BOOL)requiresUserActionForMediaPlayback
{
    return self.mediaTypesRequiringUserActionForPlayback == WKAudiovisualMediaTypeAll;
}

- (void)setRequiresUserActionForMediaPlayback:(BOOL)requiresUserActionForMediaPlayback
{
    self.mediaTypesRequiringUserActionForPlayback = requiresUserActionForMediaPlayback ? WKAudiovisualMediaTypeAll : WKAudiovisualMediaTypeNone;
}
#endif // PLATFORM(IOS_FAMILY)

@end

@implementation WKWebViewConfiguration (WKPrivateDeprecated)

#if PLATFORM(IOS_FAMILY)
- (BOOL)_textInteractionGesturesEnabled
{
    return _pageConfiguration->textInteractionGesturesEnabled();
}

- (void)_setTextInteractionGesturesEnabled:(BOOL)enabled
{
    _pageConfiguration->setTextInteractionGesturesEnabled(enabled);
}
#endif // PLATFORM(IOS_FAMILY)

@end

@implementation WKWebViewConfiguration (WKBinaryCompatibilityWithIOS10)

-(_WKVisitedLinkStore *)_visitedLinkProvider
{
    return self._visitedLinkStore;
}

- (void)_setVisitedLinkProvider:(_WKVisitedLinkStore *)visitedLinkProvider
{
    self._visitedLinkStore = visitedLinkProvider;
}

@end
