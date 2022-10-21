/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "UserInterfaceIdiom.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKProcessPool.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKUserContentController.h>
#import "WKWebpagePreferencesInternal.h"
#import <WebKit/WKWebView.h>
#import "WKWebViewContentProviderRegistry.h"
#import "WebKit2Initialize.h"
#import "WebPreferencesDefaultValues.h"
#import "WebPreferencesDefinitions.h"
#import "WebURLSchemeHandlerCocoa.h"
#import "_WKApplicationManifestInternal.h"
#import "_WKVisitedLinkStore.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Settings.h>
#import <wtf/RetainPtr.h>
#import <wtf/RobinHoodHashSet.h>
#import <wtf/URLParser.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

template<typename T> class LazyInitialized {
public:
    typedef typename WTF::GetPtrHelper<T>::PtrType PtrType;

    template<typename F>
    PtrType get(F&& f)
    {
        if (!m_isInitialized) {
            m_value = f();
            m_isInitialized = true;
        }

        return m_value.get();
    }

    void set(PtrType t)
    {
        m_value = t;
        m_isInitialized = true;
    }

    void set(T&& t)
    {
        m_value = WTFMove(t);
        m_isInitialized = true;
    }

    PtrType peek()
    {
        return m_value.get();
    }

private:
    bool m_isInitialized = false;
    T m_value;
};

#if PLATFORM(IOS_FAMILY)

static _WKDragLiftDelay toDragLiftDelay(NSUInteger value)
{
    if (value == _WKDragLiftDelayMedium)
        return _WKDragLiftDelayMedium;
    if (value == _WKDragLiftDelayLong)
        return _WKDragLiftDelayLong;
    return _WKDragLiftDelayShort;
}

static bool defaultShouldDecidePolicyBeforeLoadingQuickLookPreview()
{
#if USE(QUICK_LOOK)
    static bool shouldDecide = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DecidesPolicyBeforeLoadingQuickLookPreview);
    return shouldDecide;
#else
    return false;
#endif
}

#endif

@implementation WKWebViewConfiguration {
    RefPtr<API::PageConfiguration> _pageConfiguration;
    LazyInitialized<RetainPtr<WKProcessPool>> _processPool;
    LazyInitialized<RetainPtr<WKPreferences>> _preferences;
    LazyInitialized<RetainPtr<WKUserContentController>> _userContentController;
#if ENABLE(WK_WEB_EXTENSIONS)
    LazyInitialized<RetainPtr<_WKWebExtensionController>> _webExtensionController;
#endif
    LazyInitialized<RetainPtr<_WKVisitedLinkStore>> _visitedLinkStore;
    LazyInitialized<RetainPtr<WKWebsiteDataStore>> _websiteDataStore;
    LazyInitialized<RetainPtr<WKWebpagePreferences>> _defaultWebpagePreferences;
    WeakObjCPtr<WKWebView> _relatedWebView;
    WeakObjCPtr<WKWebView> _alternateWebViewForNavigationGestures;
    RetainPtr<NSString> _groupIdentifier;
    std::optional<RetainPtr<NSString>> _applicationNameForUserAgent;
    NSTimeInterval _incrementalRenderingSuppressionTimeout;
    BOOL _respectsImageOrientation;
    BOOL _allowsJavaScriptMarkup;
    BOOL _convertsPositionStyleOnCopy;
    BOOL _allowsMetaRefresh;
    BOOL _allowUniversalAccessFromFileURLs;
    BOOL _allowTopNavigationToDataURLs;

#if PLATFORM(IOS_FAMILY)
    LazyInitialized<RetainPtr<WKWebViewContentProviderRegistry>> _contentProviderRegistry;
    BOOL _allowsInlineMediaPlayback;
    BOOL _inlineMediaPlaybackRequiresPlaysInlineAttribute;
    BOOL _allowsInlineMediaPlaybackAfterFullscreen;
    _WKDragLiftDelay _dragLiftDelay;
    BOOL _textInteractionGesturesEnabled;
    BOOL _longPressActionsEnabled;
    BOOL _systemPreviewEnabled;
    BOOL _shouldDecidePolicyBeforeLoadingQuickLookPreview;
#endif

    BOOL _invisibleAutoplayNotPermitted;
    BOOL _mediaDataLoadsAutomatically;
    BOOL _attachmentElementEnabled;
    Class _attachmentFileWrapperClass;
    BOOL _mainContentUserGestureOverrideEnabled;

#if PLATFORM(MAC)
    WKRetainPtr<WKPageGroupRef> _pageGroup;
    BOOL _showsURLsInToolTips;
    BOOL _serviceControlsEnabled;
    BOOL _imageControlsEnabled;
#endif
    BOOL _waitsForPaintAfterViewDidMoveToWindow;
    BOOL _controlledByAutomation;

#if ENABLE(APPLE_PAY)
    BOOL _applePayEnabled;
#endif
    BOOL _needsStorageAccessFromFileURLsQuirk;
    BOOL _legacyEncryptedMediaAPIEnabled;
    BOOL _allowMediaContentTypesRequiringHardwareSupportAsFallback;
    BOOL _colorFilterEnabled;
    BOOL _incompleteImageBorderEnabled;
    BOOL _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad;
    BOOL _drawsBackground;
    BOOL _undoManagerAPIEnabled;
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

    _allowsInlineMediaPlayback = !WebKit::currentUserInterfaceIdiomIsSmallScreen();
    _inlineMediaPlaybackRequiresPlaysInlineAttribute = !_allowsInlineMediaPlayback;
    _allowsInlineMediaPlaybackAfterFullscreen = !_allowsInlineMediaPlayback;
    _mediaDataLoadsAutomatically = _allowsInlineMediaPlayback;
#if !PLATFORM(WATCHOS)
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::MediaTypesRequiringUserActionForPlayback))
        _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAudio;
    else
#endif
        _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAll;
    _ignoresViewportScaleLimits = NO;
#else
    _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    _mediaDataLoadsAutomatically = YES;
    _userInterfaceDirectionPolicy = WKUserInterfaceDirectionPolicyContent;
#endif
    _legacyEncryptedMediaAPIEnabled = YES;
    _mainContentUserGestureOverrideEnabled = NO;
    _invisibleAutoplayNotPermitted = NO;
    _attachmentElementEnabled = NO;

#if PLATFORM(IOS_FAMILY)
    _respectsImageOrientation = YES;
#endif

#if PLATFORM(MAC)
    _respectsImageOrientation = NO;
    _showsURLsInToolTips = NO;
    _serviceControlsEnabled = NO;
    _imageControlsEnabled = NO;
#endif
    _waitsForPaintAfterViewDidMoveToWindow = YES;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    _allowsAirPlayForMediaPlayback = YES;
#endif

    _incrementalRenderingSuppressionTimeout = 5;
    _allowsJavaScriptMarkup = YES;
    _convertsPositionStyleOnCopy = NO;
    _allowsMetaRefresh = YES;
    _allowUniversalAccessFromFileURLs = NO;
    _allowTopNavigationToDataURLs = NO;
    _needsStorageAccessFromFileURLsQuirk = YES;

#if PLATFORM(IOS_FAMILY)
    _selectionGranularity = WKSelectionGranularityDynamic;
    _dragLiftDelay = toDragLiftDelay([[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitDebugDragLiftDelay"]);
#if PLATFORM(WATCHOS)
    _textInteractionGesturesEnabled = NO;
    _longPressActionsEnabled = NO;
#else
    _textInteractionGesturesEnabled = YES;
    _longPressActionsEnabled = YES;
#endif
    _systemPreviewEnabled = NO;
    _shouldDecidePolicyBeforeLoadingQuickLookPreview = defaultShouldDecidePolicyBeforeLoadingQuickLookPreview();
#endif // PLATFORM(IOS_FAMILY)

    _mediaContentTypesRequiringHardwareSupport = @"";
    _allowMediaContentTypesRequiringHardwareSupportAsFallback = YES;

    _colorFilterEnabled = NO;
    _incompleteImageBorderEnabled = NO;
    _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad = YES;
    _drawsBackground = YES;

    _undoManagerAPIEnabled = NO;

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

#if ENABLE(WK_WEB_EXTENSIONS)
    if (auto *controller = self._webExtensionControllerIfExists)
        [coder encodeObject:controller forKey:@"webExtensionController"];
#endif

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

#if ENABLE(WK_WEB_EXTENSIONS)
    if ([coder containsValueForKey:@"webExtensionController"])
        self._webExtensionController = [coder decodeObjectOfClass:[_WKWebExtensionController class] forKey:@"webExtensionController"];
#endif

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
    configuration._alternateWebViewForNavigationGestures = _alternateWebViewForNavigationGestures.get().get();
#if PLATFORM(IOS_FAMILY)
    configuration._contentProviderRegistry = self._contentProviderRegistry;
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
    if (auto *controller = self._webExtensionControllerIfExists)
        configuration._webExtensionController = controller;
#endif

    configuration->_suppressesIncrementalRendering = self->_suppressesIncrementalRendering;
    configuration->_applicationNameForUserAgent = self->_applicationNameForUserAgent;

    configuration->_respectsImageOrientation = self->_respectsImageOrientation;
    configuration->_incrementalRenderingSuppressionTimeout = self->_incrementalRenderingSuppressionTimeout;
    configuration->_allowsJavaScriptMarkup = self->_allowsJavaScriptMarkup;
    configuration->_convertsPositionStyleOnCopy = self->_convertsPositionStyleOnCopy;
    configuration->_allowsMetaRefresh = self->_allowsMetaRefresh;
    configuration->_allowUniversalAccessFromFileURLs = self->_allowUniversalAccessFromFileURLs;
    configuration->_allowTopNavigationToDataURLs = self->_allowTopNavigationToDataURLs;

    configuration->_invisibleAutoplayNotPermitted = self->_invisibleAutoplayNotPermitted;
    configuration->_mediaDataLoadsAutomatically = self->_mediaDataLoadsAutomatically;
    configuration->_attachmentElementEnabled = self->_attachmentElementEnabled;
    configuration->_attachmentFileWrapperClass = self->_attachmentFileWrapperClass;
    configuration->_mediaTypesRequiringUserActionForPlayback = self->_mediaTypesRequiringUserActionForPlayback;
    configuration->_mainContentUserGestureOverrideEnabled = self->_mainContentUserGestureOverrideEnabled;
    configuration->_waitsForPaintAfterViewDidMoveToWindow = self->_waitsForPaintAfterViewDidMoveToWindow;
    configuration->_controlledByAutomation = self->_controlledByAutomation;

#if PLATFORM(IOS_FAMILY)
    configuration->_allowsInlineMediaPlayback = self->_allowsInlineMediaPlayback;
    configuration->_allowsInlineMediaPlaybackAfterFullscreen = self->_allowsInlineMediaPlaybackAfterFullscreen;
    configuration->_inlineMediaPlaybackRequiresPlaysInlineAttribute = self->_inlineMediaPlaybackRequiresPlaysInlineAttribute;
    configuration->_allowsPictureInPictureMediaPlayback = self->_allowsPictureInPictureMediaPlayback;
    configuration->_selectionGranularity = self->_selectionGranularity;
    configuration->_ignoresViewportScaleLimits = self->_ignoresViewportScaleLimits;
    configuration->_dragLiftDelay = self->_dragLiftDelay;
    configuration->_textInteractionGesturesEnabled = self->_textInteractionGesturesEnabled;
    configuration->_longPressActionsEnabled = self->_longPressActionsEnabled;
    configuration->_systemPreviewEnabled = self->_systemPreviewEnabled;
    configuration->_shouldDecidePolicyBeforeLoadingQuickLookPreview = self->_shouldDecidePolicyBeforeLoadingQuickLookPreview;
#endif
#if PLATFORM(MAC)
    configuration->_userInterfaceDirectionPolicy = self->_userInterfaceDirectionPolicy;
    configuration->_showsURLsInToolTips = self->_showsURLsInToolTips;
    configuration->_serviceControlsEnabled = self->_serviceControlsEnabled;
    configuration->_imageControlsEnabled = self->_imageControlsEnabled;
    configuration->_pageGroup = self._pageGroup;
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
    configuration->_needsStorageAccessFromFileURLsQuirk = self->_needsStorageAccessFromFileURLsQuirk;

    configuration->_mediaContentTypesRequiringHardwareSupport = adoptNS([self._mediaContentTypesRequiringHardwareSupport copyWithZone:zone]);
    configuration->_additionalSupportedImageTypes = adoptNS([self->_additionalSupportedImageTypes copyWithZone:zone]);
    configuration->_legacyEncryptedMediaAPIEnabled = self->_legacyEncryptedMediaAPIEnabled;
    configuration->_allowMediaContentTypesRequiringHardwareSupportAsFallback = self->_allowMediaContentTypesRequiringHardwareSupportAsFallback;

    configuration->_groupIdentifier = adoptNS([self->_groupIdentifier copyWithZone:zone]);
    configuration->_colorFilterEnabled = self->_colorFilterEnabled;
    configuration->_incompleteImageBorderEnabled = self->_incompleteImageBorderEnabled;
    configuration->_shouldDeferAsynchronousScriptsUntilAfterDocumentLoad = self->_shouldDeferAsynchronousScriptsUntilAfterDocumentLoad;
    configuration->_drawsBackground = self->_drawsBackground;

    configuration->_undoManagerAPIEnabled = self->_undoManagerAPIEnabled;
#if ENABLE(APP_HIGHLIGHTS)
    configuration->_appHighlightsEnabled = self->_appHighlightsEnabled;
#endif

    configuration->_sampledPageTopColorMaxDifference = self->_sampledPageTopColorMaxDifference;
    configuration->_sampledPageTopColorMinHeight = self->_sampledPageTopColorMinHeight;

    return configuration;
}

- (WKProcessPool *)processPool
{
    return _processPool.get([] { return adoptNS([[WKProcessPool alloc] init]); });
}

- (void)setProcessPool:(WKProcessPool *)processPool
{
    _processPool.set(processPool);
}

- (WKPreferences *)preferences
{
    return _preferences.get([] { return adoptNS([[WKPreferences alloc] init]); });
}

- (void)setPreferences:(WKPreferences *)preferences
{
    _preferences.set(preferences);
}

- (WKUserContentController *)userContentController
{
    return _userContentController.get([] { return adoptNS([[WKUserContentController alloc] init]); });
}

- (void)setUserContentController:(WKUserContentController *)userContentController
{
    _userContentController.set(userContentController);
}

- (_WKWebExtensionController *)_webExtensionControllerIfExists
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return _webExtensionController.peek();
#else
    return nullptr;
#endif
}

- (_WKWebExtensionController *)_webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    return _webExtensionController.get([] { return adoptNS([[_WKWebExtensionController alloc] init]); });
#else
    return nullptr;
#endif
}

- (void)_setWebExtensionController:(_WKWebExtensionController *)webExtensionController
{
#if ENABLE(WK_WEB_EXTENSIONS)
    _webExtensionController.set(webExtensionController);
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
    return _websiteDataStore.get([] { return [WKWebsiteDataStore defaultDataStore]; });
}

- (void)setWebsiteDataStore:(WKWebsiteDataStore *)websiteDataStore
{
    _websiteDataStore.set(websiteDataStore);
}

- (WKWebpagePreferences *)defaultWebpagePreferences
{
    return _defaultWebpagePreferences.get([] {
        return WKWebpagePreferences.defaultPreferences;
    });
}

- (void)setDefaultWebpagePreferences:(WKWebpagePreferences *)defaultWebpagePreferences
{
    _defaultWebpagePreferences.set(defaultWebpagePreferences ?: WKWebpagePreferences.defaultPreferences);
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
    return _visitedLinkStore.get([] { return adoptNS([[_WKVisitedLinkStore alloc] init]); });
}

- (void)_setVisitedLinkStore:(_WKVisitedLinkStore *)visitedLinkStore
{
    _visitedLinkStore.set(visitedLinkStore);
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

- (WKWebViewContentProviderRegistry *)_contentProviderRegistry
{
    return _contentProviderRegistry.get([self] { return adoptNS([[WKWebViewContentProviderRegistry alloc] initWithConfiguration:self]); });
}

- (void)_setContentProviderRegistry:(WKWebViewContentProviderRegistry *)registry
{
    _contentProviderRegistry.set(registry);
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
    return _respectsImageOrientation;
}

- (void)_setRespectsImageOrientation:(BOOL)respectsImageOrientation
{
    _respectsImageOrientation = respectsImageOrientation;
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
    return _incrementalRenderingSuppressionTimeout;
}

- (void)_setIncrementalRenderingSuppressionTimeout:(NSTimeInterval)incrementalRenderingSuppressionTimeout
{
    _incrementalRenderingSuppressionTimeout = incrementalRenderingSuppressionTimeout;
}

- (BOOL)_allowsJavaScriptMarkup
{
    return _allowsJavaScriptMarkup;
}

- (void)_setAllowsJavaScriptMarkup:(BOOL)allowsJavaScriptMarkup
{
    _allowsJavaScriptMarkup = allowsJavaScriptMarkup;
}

- (BOOL)_allowUniversalAccessFromFileURLs
{
    return _allowUniversalAccessFromFileURLs;
}

- (void)_setAllowUniversalAccessFromFileURLs:(BOOL)allowUniversalAccessFromFileURLs
{
    _allowUniversalAccessFromFileURLs = allowUniversalAccessFromFileURLs;
}

- (BOOL)_allowTopNavigationToDataURLs
{
    return _allowTopNavigationToDataURLs;
}

- (void)_setAllowTopNavigationToDataURLs:(BOOL)allowTopNavigationToDataURLs
{
    _allowTopNavigationToDataURLs = allowTopNavigationToDataURLs;
}

- (BOOL)_convertsPositionStyleOnCopy
{
    return _convertsPositionStyleOnCopy;
}

- (void)_setConvertsPositionStyleOnCopy:(BOOL)convertsPositionStyleOnCopy
{
    _convertsPositionStyleOnCopy = convertsPositionStyleOnCopy;
}

- (BOOL)_allowsMetaRefresh
{
    return _allowsMetaRefresh;
}

- (void)_setAllowsMetaRefresh:(BOOL)allowsMetaRefresh
{
    _allowsMetaRefresh = allowsMetaRefresh;
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)_clientNavigationsRunAtForegroundPriority
{
    return _pageConfiguration->clientNavigationsRunAtForegroundPriority();
}

- (void)_setClientNavigationsRunAtForegroundPriority:(BOOL)clientNavigationsRunAtForegroundPriority
{
    _pageConfiguration->setClientNavigationsRunAtForegroundPriority(clientNavigationsRunAtForegroundPriority);
}

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
    return _inlineMediaPlaybackRequiresPlaysInlineAttribute;
}

- (void)_setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)requiresPlaysInlineAttribute
{
    _inlineMediaPlaybackRequiresPlaysInlineAttribute = requiresPlaysInlineAttribute;
}

- (BOOL)_allowsInlineMediaPlaybackAfterFullscreen
{
    return _allowsInlineMediaPlaybackAfterFullscreen;
}

- (void)_setAllowsInlineMediaPlaybackAfterFullscreen:(BOOL)allows
{
    _allowsInlineMediaPlaybackAfterFullscreen = allows;
}

- (_WKDragLiftDelay)_dragLiftDelay
{
    return _dragLiftDelay;
}

- (void)_setDragLiftDelay:(_WKDragLiftDelay)dragLiftDelay
{
    _dragLiftDelay = dragLiftDelay;
}

- (BOOL)_longPressActionsEnabled
{
    return _longPressActionsEnabled;
}

- (void)_setLongPressActionsEnabled:(BOOL)enabled
{
    _longPressActionsEnabled = enabled;
}

- (BOOL)_systemPreviewEnabled
{
    return _systemPreviewEnabled;
}

- (void)_setSystemPreviewEnabled:(BOOL)enabled
{
    _systemPreviewEnabled = enabled;
}

- (BOOL)_shouldDecidePolicyBeforeLoadingQuickLookPreview
{
    return _shouldDecidePolicyBeforeLoadingQuickLookPreview;
}

- (void)_setShouldDecidePolicyBeforeLoadingQuickLookPreview:(BOOL)shouldDecide
{
    _shouldDecidePolicyBeforeLoadingQuickLookPreview = shouldDecide;
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
    return _invisibleAutoplayNotPermitted;
}

- (void)_setInvisibleAutoplayNotPermitted:(BOOL)notPermitted
{
    _invisibleAutoplayNotPermitted = notPermitted;
}

- (BOOL)_mediaDataLoadsAutomatically
{
    return _mediaDataLoadsAutomatically;
}

- (void)_setMediaDataLoadsAutomatically:(BOOL)mediaDataLoadsAutomatically
{
    _mediaDataLoadsAutomatically = mediaDataLoadsAutomatically;
}

- (BOOL)_attachmentElementEnabled
{
    return _attachmentElementEnabled;
}

- (void)_setAttachmentElementEnabled:(BOOL)attachmentElementEnabled
{
    _attachmentElementEnabled = attachmentElementEnabled;
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
    return _colorFilterEnabled;
}

- (void)_setColorFilterEnabled:(BOOL)colorFilterEnabled
{
    _colorFilterEnabled = colorFilterEnabled;
}

- (BOOL)_incompleteImageBorderEnabled
{
    return _incompleteImageBorderEnabled;
}

- (void)_setIncompleteImageBorderEnabled:(BOOL)incompleteImageBorderEnabled
{
    _incompleteImageBorderEnabled = incompleteImageBorderEnabled;
}

- (BOOL)_shouldDeferAsynchronousScriptsUntilAfterDocumentLoad
{
    return _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad;
}

- (void)_setShouldDeferAsynchronousScriptsUntilAfterDocumentLoad:(BOOL)shouldDeferAsynchronousScriptsUntilAfterDocumentLoad
{
    _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad = shouldDeferAsynchronousScriptsUntilAfterDocumentLoad;
}

- (WKWebsiteDataStore *)_websiteDataStoreIfExists
{
    return _websiteDataStore.peek();
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
    return _drawsBackground;
}

- (void)_setDrawsBackground:(BOOL)drawsBackground
{
    _drawsBackground = drawsBackground;
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
    return _mainContentUserGestureOverrideEnabled;
}

- (void)_setMainContentUserGestureOverrideEnabled:(BOOL)mainContentUserGestureOverrideEnabled
{
    _mainContentUserGestureOverrideEnabled = mainContentUserGestureOverrideEnabled;
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
    return _waitsForPaintAfterViewDidMoveToWindow;
}

- (void)_setWaitsForPaintAfterViewDidMoveToWindow:(BOOL)shouldSynchronize
{
    _waitsForPaintAfterViewDidMoveToWindow = shouldSynchronize;
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
    return _showsURLsInToolTips;
}

- (void)_setShowsURLsInToolTips:(BOOL)showsURLsInToolTips
{
    _showsURLsInToolTips = showsURLsInToolTips;
}

- (BOOL)_serviceControlsEnabled
{
    return _serviceControlsEnabled;
}

- (void)_setServiceControlsEnabled:(BOOL)serviceControlsEnabled
{
    _serviceControlsEnabled = serviceControlsEnabled;
}

- (BOOL)_imageControlsEnabled
{
    return _imageControlsEnabled;
}

- (void)_setImageControlsEnabled:(BOOL)imageControlsEnabled
{
    _imageControlsEnabled = imageControlsEnabled;
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
    return _pageGroup.get();
}

- (void)_setPageGroup:(WKPageGroupRef)pageGroup
{
    _pageGroup = pageGroup;
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
    return _needsStorageAccessFromFileURLsQuirk;
}

- (void)_setNeedsStorageAccessFromFileURLsQuirk:(BOOL)needsLocalStorageQuirk
{
    _needsStorageAccessFromFileURLsQuirk = needsLocalStorageQuirk;
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
    _legacyEncryptedMediaAPIEnabled = enabled;
}

- (BOOL)_legacyEncryptedMediaAPIEnabled
{
    return _legacyEncryptedMediaAPIEnabled;
}

- (void)_setAllowMediaContentTypesRequiringHardwareSupportAsFallback:(BOOL)allow
{
    _allowMediaContentTypesRequiringHardwareSupportAsFallback = allow;
}

- (BOOL)_allowMediaContentTypesRequiringHardwareSupportAsFallback
{
    return _allowMediaContentTypesRequiringHardwareSupportAsFallback;
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
    _undoManagerAPIEnabled = enabled;
}

- (BOOL)_undoManagerAPIEnabled
{
    return _undoManagerAPIEnabled;
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

- (BOOL)_shouldRelaxThirdPartyCookieBlocking
{
    return _pageConfiguration->shouldRelaxThirdPartyCookieBlocking() == WebCore::ShouldRelaxThirdPartyCookieBlocking::Yes;
}

- (void)_setShouldRelaxThirdPartyCookieBlocking:(BOOL)relax
{
    bool allowed = WebCore::applicationBundleIdentifier() == "com.apple.WebKit.TestWebKitAPI"_s;
#if PLATFORM(MAC)
    allowed = allowed || WebCore::MacApplication::isSafari();
#elif PLATFORM(IOS_FAMILY)
    allowed = allowed || WebCore::IOSApplication::isMobileSafari() || WebCore::IOSApplication::isSafariViewService();
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
    return _textInteractionGesturesEnabled;
}

- (void)_setTextInteractionGesturesEnabled:(BOOL)enabled
{
    _textInteractionGesturesEnabled = enabled;
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
