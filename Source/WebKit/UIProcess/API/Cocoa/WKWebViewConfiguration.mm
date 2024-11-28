/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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
#import "PlatformWritingToolsUtilities.h"
#import "WKDataDetectorTypesInternal.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKUserContentControllerInternal.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WKWebViewInternal.h"
#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebKit2Initialize.h"
#import "WebPageProxy.h"
#import "WebPreferencesDefinitions.h"
#import "WebURLSchemeHandlerCocoa.h"
#import "_WKApplicationManifestInternal.h"
#import "_WKVisitedLinkStoreInternal.h"
#import <WebCore/Settings.h>
#import <WebCore/WebCoreObjCExtras.h>
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
#import "WKWebExtensionControllerInternal.h"
#import "_WKWebExtensionController.h"
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
#import <WebCore/ShouldRequireExplicitConsentForGamepadAccess.h>
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

@implementation WKWebViewConfiguration

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::PageConfiguration>(self);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWebViewConfiguration.class, self))
        return;

    _pageConfiguration->API::PageConfiguration::~PageConfiguration();

    [super dealloc];
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

- (WKSelectionGranularity)selectionGranularity
{
    return _pageConfiguration->selectionGranularity() == WebKit::SelectionGranularity::Character ? WKSelectionGranularityCharacter : WKSelectionGranularityDynamic;
}

- (void)setSelectionGranularity:(WKSelectionGranularity)granularity
{
    _pageConfiguration->setSelectionGranularity(granularity == WKSelectionGranularityCharacter ? WebKit::SelectionGranularity::Character : WebKit::SelectionGranularity::Dynamic);
}

- (void)setAllowsPictureInPictureMediaPlayback:(BOOL)allows
{
    _pageConfiguration->setAllowsPictureInPictureMediaPlayback(allows);
}

- (BOOL)allowsPictureInPictureMediaPlayback
{
    return _pageConfiguration->allowsPictureInPictureMediaPlayback();
}

- (void)setIgnoresViewportScaleLimits:(BOOL)ignores
{
    _pageConfiguration->setIgnoresViewportScaleLimits(ignores);
}

- (BOOL)ignoresViewportScaleLimits
{
    return _pageConfiguration->ignoresViewportScaleLimits();
}

- (void)setDataDetectorTypes:(WKDataDetectorTypes)types
{
#if ENABLE(DATA_DETECTION)
    _pageConfiguration->setDataDetectorTypes(fromWKDataDetectorTypes(types));
#endif
}

- (WKDataDetectorTypes)dataDetectorTypes
{
#if ENABLE(DATA_DETECTION)
    return toWKDataDetectorTypes(_pageConfiguration->dataDetectorTypes());
#else
    return WKDataDetectorTypeNone;
#endif
}

#else // PLATFORM(IOS_FAMILY)

- (WKUserInterfaceDirectionPolicy)userInterfaceDirectionPolicy
{
    return _pageConfiguration->userInterfaceDirectionPolicy() == WebCore::UserInterfaceDirectionPolicy::System ? WKUserInterfaceDirectionPolicySystem : WKUserInterfaceDirectionPolicyContent;
}

- (void)setUserInterfaceDirectionPolicy:(WKUserInterfaceDirectionPolicy)policy
{
    return _pageConfiguration->setUserInterfaceDirectionPolicy(policy == WKUserInterfaceDirectionPolicySystem ? WebCore::UserInterfaceDirectionPolicy::System : WebCore::UserInterfaceDirectionPolicy::Content);
}

#endif // PLATFORM(IOS_FAMILY)

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

    if (auto& applicationNameForUserAgent = _pageConfiguration->applicationNameForUserAgent())
        [coder encodeObject:*applicationNameForUserAgent forKey:@"applicationNameForUserAgent"];

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
    [coder encodeBool:self._scrollToTextFragmentIndicatorEnabled forKey:@"scrollToTextFragmentIndicatorEnabled"];
    [coder encodeBool:self._scrollToTextFragmentMarkingEnabled forKey:@"scrollToTextFragmentMarkingEnabled"];
    [coder encodeBool:self._multiRepresentationHEICInsertionEnabled forKey:@"multiRepresentationHEICInsertionEnabled"];
#if PLATFORM(VISION)
    [coder encodeBool:self._gamepadAccessRequiresExplicitConsent forKey:@"gamepadAccessRequiresExplicitConsent"];
    [coder encodeBool:self._overlayRegionsEnabled forKey:@"overlayRegionsEnabled"];
    [coder encodeBool:self._cssTransformStyleSeparatedEnabled forKey:@"cssTransformStyleSeparatedEnabled"];
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
    auto selectionGranularityCandidate = static_cast<WKSelectionGranularity>([coder decodeIntegerForKey:@"selectionGranularity"]);
    if (selectionGranularityCandidate == WKSelectionGranularityDynamic || selectionGranularityCandidate == WKSelectionGranularityCharacter)
        self.selectionGranularity = selectionGranularityCandidate;
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
    self._scrollToTextFragmentIndicatorEnabled = [coder decodeBoolForKey:@"scrollToTextFragmentIndicatorEnabled"];
    self._scrollToTextFragmentMarkingEnabled = [coder decodeBoolForKey:@"scrollToTextFragmentMarkingEnabled"];
    self._multiRepresentationHEICInsertionEnabled = [coder decodeBoolForKey:@"multiRepresentationHEICInsertionEnabled"];
#if PLATFORM(VISION)
    self._gamepadAccessRequiresExplicitConsent = [coder decodeBoolForKey:@"gamepadAccessRequiresExplicitConsent"];
    self._overlayRegionsEnabled = [coder decodeBoolForKey:@"overlayRegionsEnabled"];
    self._cssTransformStyleSeparatedEnabled = [coder decodeBoolForKey:@"cssTransformStyleSeparatedEnabled"];
#endif

    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    WKWebViewConfiguration *configuration = [(WKWebViewConfiguration *)[[self class] allocWithZone:zone] init];
    configuration->_pageConfiguration->copyDataFrom(*_pageConfiguration);
    return configuration;
}

- (void)setValue:(id)value forKey:(NSString *)key
{
    if (([key isEqualToString:@"allowUniversalAccessFromFileURLs"] || [key isEqualToString:@"_allowUniversalAccessFromFileURLs"]) && [value isKindOfClass:[NSNumber class]] && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ThrowOnKVCInstanceVariableAccess)) {
        RELEASE_LOG_FAULT(API, "Do not mutate private state `%{public}@` via KVC. Doing this when linking against newer SDKs will result in a crash.", key);
        [self _setAllowUniversalAccessFromFileURLs:[(NSNumber *)value boolValue]];
        return;
    }

    [super setValue:value forKey:key];
}

- (id)valueForKey:(NSString *)key
{
    if (([key isEqualToString:@"allowUniversalAccessFromFileURLs"] || [key isEqualToString:@"_allowUniversalAccessFromFileURLs"]) && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ThrowOnKVCInstanceVariableAccess)) {
        RELEASE_LOG_FAULT(API, "Do not access private state `%{public}@` via KVC. Doing this when linking against newer SDKs will result in a crash.", key);
        return @([self _allowUniversalAccessFromFileURLs]);
    }

    return [super valueForKey:key];
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

- (WKWebExtensionController *)_strongWebExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return wrapper(_pageConfiguration->webExtensionController());
#else
    return nil;
#endif
}

- (WKWebExtensionController *)_weakWebExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return wrapper(_pageConfiguration->weakWebExtensionController());
#else
    return nil;
#endif
}

- (void)_setWeakWebExtensionController:(WKWebExtensionController *)webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    _pageConfiguration->setWeakWebExtensionController(webExtensionController ? &webExtensionController._webExtensionController : nullptr);
#endif
}

- (WKWebExtensionController *)webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return self._weakWebExtensionController ?: self._strongWebExtensionController;
#else
    return nil;
#endif
}

- (void)setWebExtensionController:(WKWebExtensionController *)webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    _pageConfiguration->setWebExtensionController(webExtensionController ? &webExtensionController._webExtensionController : nullptr);
#endif
}

- (_WKWebExtensionController *)_webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return (_WKWebExtensionController *)self.webExtensionController;
#else
    return nil;
#endif
}

- (void)_setWebExtensionController:(_WKWebExtensionController *)webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    self.webExtensionController = webExtensionController;
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

- (WKAudiovisualMediaTypes)mediaTypesRequiringUserActionForPlayback
{
    return _pageConfiguration->mediaTypesRequiringUserActionForPlayback();
}

- (void)setMediaTypesRequiringUserActionForPlayback:(WKAudiovisualMediaTypes)types
{
    _pageConfiguration->setMediaTypesRequiringUserActionForPlayback(types);
}

- (BOOL)suppressesIncrementalRendering
{
    return _pageConfiguration->suppressesIncrementalRendering();
}

- (void)setSuppressesIncrementalRendering:(BOOL)suppresses
{
    _pageConfiguration->setSuppressesIncrementalRendering(suppresses);
}

- (void)setAllowsAirPlayForMediaPlayback:(BOOL)allows
{
    _pageConfiguration->setAllowsAirPlayForMediaPlayback(allows);
}

- (BOOL)allowsAirPlayForMediaPlayback
{
    return _pageConfiguration->allowsAirPlayForMediaPlayback();
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
    return nsStringNilIfNull(_pageConfiguration->applicationNameForUserAgent().value_or(String()));
}

- (NSString *)applicationNameForUserAgent
{
    return nsStringNilIfNull(_pageConfiguration->applicationNameForUserAgent().value_or(defaultApplicationNameForUserAgent()));
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationNameForUserAgent
{
    _pageConfiguration->setApplicationNameForUserAgent(applicationNameForUserAgent);
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

#if ENABLE(WRITING_TOOLS)

- (void)setSupportsAdaptiveImageGlyph:(BOOL)supportsAdaptiveImageGlyph
{
    [self _setMultiRepresentationHEICInsertionEnabled:supportsAdaptiveImageGlyph];
}

- (BOOL)supportsAdaptiveImageGlyph
{
    return [self _multiRepresentationHEICInsertionEnabled];
}

- (void)setWritingToolsBehavior:(PlatformWritingToolsBehavior)writingToolsBehavior
{
    _pageConfiguration->setWritingToolsBehavior(WebKit::convertToWebWritingToolsBehavior(writingToolsBehavior));
}

- (PlatformWritingToolsBehavior)writingToolsBehavior
{
    return WebKit::convertToPlatformWritingToolsBehavior(_pageConfiguration->writingToolsBehavior());
}

#endif // ENABLE(WRITING_TOOLS)

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_pageConfiguration;
}

@end

@implementation WKWebViewConfiguration (WKPrivate)

- (WKWebView *)_relatedWebView
{
    // FIXME: Remove when rdar://134318457, rdar://134318538 and rdar://125369363 are complete.
    if (RefPtr page = _pageConfiguration->relatedPage())
        return page->cocoaView().autorelease();
    return nil;
}

- (void)_setRelatedWebView:(WKWebView *)relatedWebView
{
    if (relatedWebView)
        _pageConfiguration->setRelatedPage(relatedWebView->_page.get());
    else
        _pageConfiguration->setRelatedPage(nullptr);
}

- (WKWebView *)_webViewToCloneSessionStorageFrom
{
    if (RefPtr page = _pageConfiguration->pageToCloneSessionStorageFrom())
        return page->cocoaView().autorelease();
    return nil;
}

- (void)_setWebViewToCloneSessionStorageFrom:(WKWebView *)webViewToCloneSessionStorageFrom
{
    if (webViewToCloneSessionStorageFrom)
        _pageConfiguration->setPageToCloneSessionStorageFrom(webViewToCloneSessionStorageFrom->_page.get());
    else
        _pageConfiguration->setPageToCloneSessionStorageFrom(nullptr);
}

- (WKWebView *)_alternateWebViewForNavigationGestures
{
    if (RefPtr page = _pageConfiguration->alternateWebViewForNavigationGestures())
        return page->cocoaView().autorelease();
    return nil;
}

- (void)_setAlternateWebViewForNavigationGestures:(WKWebView *)alternateView
{
    if (alternateView)
        _pageConfiguration->setAlternateWebViewForNavigationGestures(alternateView->_page.get());
    else
        _pageConfiguration->setAlternateWebViewForNavigationGestures(nullptr);
}

- (NSString *)_groupIdentifier
{
    return nsStringNilIfNull(_pageConfiguration->groupIdentifier());
}

- (void)_setGroupIdentifier:(NSString *)groupIdentifier
{
    _pageConfiguration->setGroupIdentifier(groupIdentifier);
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
    return _pageConfiguration->attachmentFileWrapperClass();
}

- (void)_setAttachmentFileWrapperClass:(Class)attachmentFileWrapperClass
{
    if (attachmentFileWrapperClass && ![attachmentFileWrapperClass isSubclassOfClass:[NSFileWrapper class]])
        [NSException raise:NSInvalidArgumentException format:@"Class %@ does not inherit from NSFileWrapper", attachmentFileWrapperClass];

    _pageConfiguration->setAttachmentFileWrapperClass(attachmentFileWrapperClass);
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
    return _pageConfiguration->isControlledByAutomation();
}

- (void)_setControlledByAutomation:(BOOL)controlledByAutomation
{
    _pageConfiguration->setControlledByAutomation(controlledByAutomation);
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
    return _pageConfiguration->applePayEnabled();
#else
    return NO;
#endif
}

- (void)_setApplePayEnabled:(BOOL)applePayEnabled
{
#if ENABLE(APPLE_PAY)
    _pageConfiguration->setApplePayEnabled(applePayEnabled);
#endif
}

- (BOOL)_scrollToTextFragmentIndicatorEnabled
{
    return _pageConfiguration->scrollToTextFragmentIndicatorEnabled();
}

- (void)_setScrollToTextFragmentIndicatorEnabled:(BOOL)enabled
{
    _pageConfiguration->setScrollToTextFragmentIndicatorEnabled(enabled);
}

- (BOOL)_scrollToTextFragmentMarkingEnabled
{
    return _pageConfiguration->scrollToTextFragmentMarkingEnabled();
}

- (void)_setScrollToTextFragmentMarkingEnabled:(BOOL)enabled
{
    _pageConfiguration->setScrollToTextFragmentMarkingEnabled(enabled);
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
    return _pageConfiguration->mediaContentTypesRequiringHardwareSupport();
}

- (void)_setMediaContentTypesRequiringHardwareSupport:(NSString *)mediaContentTypesRequiringHardwareSupport
{
    _pageConfiguration->setMediaContentTypesRequiringHardwareSupport(mediaContentTypesRequiringHardwareSupport);
}

- (NSArray<NSString *> *)_additionalSupportedImageTypes
{
    auto& types = _pageConfiguration->additionalSupportedImageTypes();
    if (!types)
        return nil;
    return createNSArray(*types).autorelease();
}

- (void)_setAdditionalSupportedImageTypes:(NSArray<NSString *> *)additionalSupportedImageTypes
{
    if (additionalSupportedImageTypes)
        _pageConfiguration->setAdditionalSupportedImageTypes(makeVector<String>(additionalSupportedImageTypes));
    else
        _pageConfiguration->setAdditionalSupportedImageTypes(std::nullopt);
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
    _pageConfiguration->setAppHighlightsEnabled(enabled);
#endif
}

- (BOOL)_appHighlightsEnabled
{
#if ENABLE(APP_HIGHLIGHTS)
    return _pageConfiguration->appHighlightsEnabled();
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
    bool allowed = applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s;
#if PLATFORM(MAC)
    allowed |= WTF::MacApplication::isSafari();
#elif PLATFORM(IOS_FAMILY)
    allowed |= WTF::IOSApplication::isMobileSafari() || WTF::IOSApplication::isSafariViewService();
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
    _pageConfiguration->setSampledPageTopColorMaxDifference(value);
}

- (double)_sampledPageTopColorMaxDifference
{
    return _pageConfiguration->sampledPageTopColorMaxDifference();
}

- (void)_setSampledPageTopColorMinHeight:(double)value
{
    _pageConfiguration->setSampledPageTopColorMinHeight(value);
}

- (double)_sampledPageTopColorMinHeight
{
    return _pageConfiguration->sampledPageTopColorMinHeight();
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

- (void)_setMultiRepresentationHEICInsertionEnabled:(BOOL)enabled
{
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    _pageConfiguration->setMultiRepresentationHEICInsertionEnabled(enabled);
#endif
}

- (BOOL)_multiRepresentationHEICInsertionEnabled
{
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    return _pageConfiguration->multiRepresentationHEICInsertionEnabled();
#else
    return NO;
#endif
}

#if PLATFORM(VISION)
- (BOOL)_gamepadAccessRequiresExplicitConsent
{
#if ENABLE(GAMEPAD)
    return _pageConfiguration->gamepadAccessRequiresExplicitConsent() == WebCore::ShouldRequireExplicitConsentForGamepadAccess::Yes;
#else
    return NO;
#endif
}

- (void)_setGamepadAccessRequiresExplicitConsent:(BOOL)gamepadAccessRequiresExplicitConsent
{
#if ENABLE(GAMEPAD)
    _pageConfiguration->setGamepadAccessRequiresExplicitConsent(gamepadAccessRequiresExplicitConsent ? WebCore::ShouldRequireExplicitConsentForGamepadAccess::Yes : WebCore::ShouldRequireExplicitConsentForGamepadAccess::No);
#endif
}

- (BOOL)_overlayRegionsEnabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    return _pageConfiguration->overlayRegionsEnabled();
#else
    return NO;
#endif
}

- (void)_setOverlayRegionsEnabled:(BOOL)overlayRegionsEnabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    _pageConfiguration->setOverlayRegionsEnabled(overlayRegionsEnabled);
#endif
}

- (BOOL)_cssTransformStyleSeparatedEnabled
{
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    return _pageConfiguration->cssTransformStyleSeparatedEnabled();
#else
    return NO;
#endif
}
- (void)_setCSSTransformStyleSeparatedEnabled:(BOOL)enabled
{
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    _pageConfiguration->setCSSTransformStyleSeparatedEnabled(enabled);
#endif
}

#endif // PLATFORM(VISION)

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
