/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>

#import "APIPageConfiguration.h"
#import "VersionChecks.h"
#import "WKPreferences.h"
#import "WKProcessPool.h"
#import "WKRetainPtr.h"
#import "WKUserContentController.h"
#import "WKWebView.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WebKit2Initialize.h"
#import "WebURLSchemeHandlerCocoa.h"
#import "_WKApplicationManifestInternal.h"
#import "_WKVisitedLinkStore.h"
#import "_WKWebsiteDataStoreInternal.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Settings.h>
#import <wtf/RetainPtr.h>
#import <wtf/URLParser.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <WebCore/Device.h>
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
    static bool shouldDecide = linkedOnOrAfter(WebKit::SDKVersion::FirstThatDecidesPolicyBeforeLoadingQuickLookPreview);
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
    LazyInitialized<RetainPtr<_WKVisitedLinkStore>> _visitedLinkStore;
    LazyInitialized<RetainPtr<WKWebsiteDataStore>> _websiteDataStore;
    LazyInitialized<RetainPtr<WKWebpagePreferences>> _defaultWebpagePreferences;
    WeakObjCPtr<WKWebView> _relatedWebView;
    WeakObjCPtr<WKWebView> _alternateWebViewForNavigationGestures;
    RetainPtr<NSString> _groupIdentifier;
    Optional<RetainPtr<NSString>> _applicationNameForUserAgent;
    NSTimeInterval _incrementalRenderingSuppressionTimeout;
    BOOL _respectsImageOrientation;
    BOOL _printsBackgrounds;
    BOOL _allowsJavaScriptMarkup;
    BOOL _convertsPositionStyleOnCopy;
    BOOL _allowsMetaRefresh;
    BOOL _allowUniversalAccessFromFileURLs;

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
    BOOL _requiresUserActionForEditingControlsManager;
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
    BOOL _editableImagesEnabled;
    BOOL _undoManagerAPIEnabled;

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
    _allowsInlineMediaPlayback = WebCore::deviceClass() == MGDeviceClassiPad;
    _inlineMediaPlaybackRequiresPlaysInlineAttribute = !_allowsInlineMediaPlayback;
    _allowsInlineMediaPlaybackAfterFullscreen = !_allowsInlineMediaPlayback;
    _mediaDataLoadsAutomatically = NO;
#if !PLATFORM(WATCHOS)
    if (WebKit::linkedOnOrAfter(WebKit::SDKVersion::FirstWithMediaTypesRequiringUserActionForPlayback))
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
    _printsBackgrounds = YES;
#endif

#if PLATFORM(MAC)
    _printsBackgrounds = NO;
    _respectsImageOrientation = NO;
    _showsURLsInToolTips = NO;
    _serviceControlsEnabled = NO;
    _imageControlsEnabled = NO;
    _requiresUserActionForEditingControlsManager = NO;
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

    _mediaContentTypesRequiringHardwareSupport = WebCore::Settings::defaultMediaContentTypesRequiringHardwareSupport();
    _allowMediaContentTypesRequiringHardwareSupportAsFallback = YES;

    _colorFilterEnabled = NO;
    _incompleteImageBorderEnabled = NO;
    _shouldDeferAsynchronousScriptsUntilAfterDocumentLoad = NO;
    _drawsBackground = YES;

    _editableImagesEnabled = NO;
    _undoManagerAPIEnabled = NO;

#if ENABLE(APPLE_PAY)
    _applePayEnabled = DEFAULT_APPLE_PAY_ENABLED;
#endif

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

    [coder encodeBool:self.suppressesIncrementalRendering forKey:@"suppressesIncrementalRendering"];

    if (_applicationNameForUserAgent.hasValue())
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
    [coder encodeBool:self._textInteractionGesturesEnabled forKey:@"textInteractionGesturesEnabled"];
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

    self.processPool = decodeObjectOfClassForKeyFromCoder([WKProcessPool class], @"processPool", coder);
    self.preferences = decodeObjectOfClassForKeyFromCoder([WKPreferences class], @"preferences", coder);
    self.userContentController = decodeObjectOfClassForKeyFromCoder([WKUserContentController class], @"userContentController", coder);
    self.websiteDataStore = decodeObjectOfClassForKeyFromCoder([WKWebsiteDataStore class], @"websiteDataStore", coder);

    self.suppressesIncrementalRendering = [coder decodeBoolForKey:@"suppressesIncrementalRendering"];

    if ([coder containsValueForKey:@"applicationNameForUserAgent"])
        self.applicationNameForUserAgent = decodeObjectOfClassForKeyFromCoder([NSString class], @"applicationNameForUserAgent", coder);

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
    self._textInteractionGesturesEnabled = [coder decodeBoolForKey:@"textInteractionGesturesEnabled"];
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
    configuration.websiteDataStore = self.websiteDataStore;
    configuration.defaultWebpagePreferences = self.defaultWebpagePreferences;
    configuration._visitedLinkStore = self._visitedLinkStore;
    configuration._relatedWebView = _relatedWebView.get().get();
    configuration._alternateWebViewForNavigationGestures = _alternateWebViewForNavigationGestures.get().get();
#if PLATFORM(IOS_FAMILY)
    configuration._contentProviderRegistry = self._contentProviderRegistry;
#endif

    configuration->_suppressesIncrementalRendering = self->_suppressesIncrementalRendering;
    configuration->_applicationNameForUserAgent = self->_applicationNameForUserAgent;

    configuration->_respectsImageOrientation = self->_respectsImageOrientation;
    configuration->_printsBackgrounds = self->_printsBackgrounds;
    configuration->_incrementalRenderingSuppressionTimeout = self->_incrementalRenderingSuppressionTimeout;
    configuration->_allowsJavaScriptMarkup = self->_allowsJavaScriptMarkup;
    configuration->_convertsPositionStyleOnCopy = self->_convertsPositionStyleOnCopy;
    configuration->_allowsMetaRefresh = self->_allowsMetaRefresh;
    configuration->_allowUniversalAccessFromFileURLs = self->_allowUniversalAccessFromFileURLs;

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
    configuration->_requiresUserActionForEditingControlsManager = self->_requiresUserActionForEditingControlsManager;
    configuration->_pageGroup = self._pageGroup;
#endif
#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
    configuration->_dataDetectorTypes = self->_dataDetectorTypes;
#endif
#if ENABLE(WIRELESS_TARGET_PLAYBACK)
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

    configuration->_editableImagesEnabled = self->_editableImagesEnabled;
    configuration->_undoManagerAPIEnabled = self->_undoManagerAPIEnabled;

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
    return _applicationNameForUserAgent.valueOr(nil).get();
}

- (NSString *)applicationNameForUserAgent
{
    return _applicationNameForUserAgent.valueOr(defaultApplicationNameForUserAgent()).get();
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

    auto canonicalScheme = WTF::URLParser::maybeCanonicalizeScheme(urlScheme);
    if (!canonicalScheme)
        [NSException raise:NSInvalidArgumentException format:@"'%@' is not a valid URL scheme", urlScheme];

    if (_pageConfiguration->urlSchemeHandlerForURLScheme(*canonicalScheme))
        [NSException raise:NSInvalidArgumentException format:@"URL scheme '%@' already has a registered URL scheme handler", urlScheme];

    _pageConfiguration->setURLSchemeHandlerForURLScheme(WebKit::WebURLSchemeHandlerCocoa::create(urlSchemeHandler), *canonicalScheme);
}

- (id <WKURLSchemeHandler>)urlSchemeHandlerForURLScheme:(NSString *)urlScheme
{
    auto canonicalScheme = WTF::URLParser::maybeCanonicalizeScheme(urlScheme);
    if (!canonicalScheme)
        return nil;

    auto handler = _pageConfiguration->urlSchemeHandlerForURLScheme(*canonicalScheme);
    return handler ? static_cast<WebKit::WebURLSchemeHandlerCocoa*>(handler.get())->apiHandler() : nil;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (_WKWebsiteDataStore *)_websiteDataStore
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return self.websiteDataStore ? adoptNS([[_WKWebsiteDataStore alloc] initWithDataStore:self.websiteDataStore]).autorelease() : nullptr;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)_setWebsiteDataStore:(_WKWebsiteDataStore *)websiteDataStore
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    self.websiteDataStore = websiteDataStore ? websiteDataStore->_dataStore.get() : nullptr;
}

ALLOW_DEPRECATED_DECLARATIONS_END

#if PLATFORM(IOS_FAMILY)
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

- (BOOL)_treatsSHA1SignedCertificatesAsInsecure
{
    return _pageConfiguration->treatsSHA1SignedCertificatesAsInsecure();
}

- (void)_setTreatsSHA1SignedCertificatesAsInsecure:(BOOL)insecure
{
    _pageConfiguration->setTreatsSHA1SignedCertificatesAsInsecure(insecure);
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
    return _printsBackgrounds;
}

- (void)_setPrintsBackgrounds:(BOOL)printsBackgrounds
{
    _printsBackgrounds = printsBackgrounds;
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
- (BOOL)_alwaysRunsAtForegroundPriority
{
    return _pageConfiguration->alwaysRunsAtForegroundPriority();
}

- (void)_setAlwaysRunsAtForegroundPriority:(BOOL)alwaysRunsAtForegroundPriority
{
    _pageConfiguration->setAlwaysRunsAtForegroundPriority(alwaysRunsAtForegroundPriority);
}

- (BOOL)_inlineMediaPlaybackRequiresPlaysInlineAttribute
{
    return _inlineMediaPlaybackRequiresPlaysInlineAttribute;
}

- (void)_setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)requires
{
    _inlineMediaPlaybackRequiresPlaysInlineAttribute = requires;
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

- (BOOL)_textInteractionGesturesEnabled
{
    return _textInteractionGesturesEnabled;
}

- (void)_setTextInteractionGesturesEnabled:(BOOL)enabled
{
    _textInteractionGesturesEnabled = enabled;
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

#endif // PLATFORM(IOS_FAMILY)

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
    if (attachmentFileWrapperClass && ![attachmentFileWrapperClass isSubclassOfClass:[NSFileWrapper self]])
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
    return _requiresUserActionForEditingControlsManager;
}

- (void)_setRequiresUserActionForEditingControlsManager:(BOOL)requiresUserAction
{
    _requiresUserActionForEditingControlsManager = requiresUserAction;
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
    return _pageConfiguration->cpuLimit().valueOr(0);
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

- (void)_setEditableImagesEnabled:(BOOL)enabled
{
    _editableImagesEnabled = enabled;
}

- (BOOL)_editableImagesEnabled
{
    return _editableImagesEnabled;
}

- (void)_setUndoManagerAPIEnabled:(BOOL)enabled
{
    _undoManagerAPIEnabled = enabled;
}

- (BOOL)_undoManagerAPIEnabled
{
    return _undoManagerAPIEnabled;
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
