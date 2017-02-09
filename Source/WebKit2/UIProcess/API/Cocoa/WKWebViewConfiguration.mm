/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

#import "APIPageConfiguration.h"
#import "VersionChecks.h"
#import "WKPreferences.h"
#import "WKProcessPool.h"
#import "WKUserContentController.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WeakObjCPtr.h"
#import "_WKVisitedLinkStore.h"
#import "_WKWebsiteDataStoreInternal.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import "UIKitSPI.h"
#import <WebCore/Device.h>
#endif

using namespace WebCore;

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

@implementation WKWebViewConfiguration {
    LazyInitialized<RetainPtr<WKProcessPool>> _processPool;
    LazyInitialized<RetainPtr<WKPreferences>> _preferences;
    LazyInitialized<RetainPtr<WKUserContentController>> _userContentController;
    LazyInitialized<RetainPtr<_WKVisitedLinkStore>> _visitedLinkStore;
    LazyInitialized<RetainPtr<WKWebsiteDataStore>> _websiteDataStore;
    WebKit::WeakObjCPtr<WKWebView> _relatedWebView;
    WebKit::WeakObjCPtr<WKWebView> _alternateWebViewForNavigationGestures;
    RetainPtr<NSString> _groupIdentifier;
    LazyInitialized<RetainPtr<NSString>> _applicationNameForUserAgent;
    NSTimeInterval _incrementalRenderingSuppressionTimeout;
    BOOL _treatsSHA1SignedCertificatesAsInsecure;
    BOOL _respectsImageOrientation;
    BOOL _printsBackgrounds;
    BOOL _allowsJavaScriptMarkup;
    BOOL _convertsPositionStyleOnCopy;
    BOOL _allowsMetaRefresh;
    BOOL _allowUniversalAccessFromFileURLs;

#if PLATFORM(IOS)
    LazyInitialized<RetainPtr<WKWebViewContentProviderRegistry>> _contentProviderRegistry;
    BOOL _alwaysRunsAtForegroundPriority;
    BOOL _allowsInlineMediaPlayback;
    BOOL _inlineMediaPlaybackRequiresPlaysInlineAttribute;
    BOOL _allowsInlineMediaPlaybackAfterFullscreen;
#endif

    BOOL _invisibleAutoplayNotPermitted;
    BOOL _mediaDataLoadsAutomatically;
    BOOL _attachmentElementEnabled;
    BOOL _mainContentUserGestureOverrideEnabled;

#if PLATFORM(MAC)
    BOOL _showsURLsInToolTips;
    BOOL _serviceControlsEnabled;
    BOOL _imageControlsEnabled;
    BOOL _requiresUserActionForEditingControlsManager;
#endif
    BOOL _initialCapitalizationEnabled;
    BOOL _waitsForPaintAfterViewDidMoveToWindow;
    BOOL _controlledByAutomation;

#if ENABLE(APPLE_PAY)
    BOOL _applePayEnabled;
#endif
    BOOL _needsStorageAccessFromFileURLsQuirk;

    NSString *_overrideContentSecurityPolicy;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
#if PLATFORM(IOS)
    _allowsPictureInPictureMediaPlayback = YES;
    _allowsInlineMediaPlayback = WebCore::deviceClass() == MGDeviceClassiPad;
    _inlineMediaPlaybackRequiresPlaysInlineAttribute = !_allowsInlineMediaPlayback;
    _allowsInlineMediaPlaybackAfterFullscreen = !_allowsInlineMediaPlayback;
    _mediaDataLoadsAutomatically = NO;
    if (linkedOnOrAfter(WebKit::SDKVersion::FirstWithMediaTypesRequiringUserActionForPlayback))
        _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAudio;
    else
        _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAll;
    _ignoresViewportScaleLimits = NO;
#else
    _mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    _mediaDataLoadsAutomatically = YES;
    _userInterfaceDirectionPolicy = WKUserInterfaceDirectionPolicyContent;
#endif
    _mainContentUserGestureOverrideEnabled = NO;
    _invisibleAutoplayNotPermitted = NO;

// FIXME: <rdar://problem/25135244> Should default to NO once clients have adopted the setting.
#if PLATFORM(IOS)
    _attachmentElementEnabled = IOSApplication::isMobileMail();
#else
    _attachmentElementEnabled = MacApplication::isAppleMail();
#endif

#if PLATFORM(IOS)
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
    _initialCapitalizationEnabled = YES;
    _waitsForPaintAfterViewDidMoveToWindow = YES;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    _allowsAirPlayForMediaPlayback = YES;
#endif

    _incrementalRenderingSuppressionTimeout = 5;
    _allowsJavaScriptMarkup = YES;
    _convertsPositionStyleOnCopy = NO;
    _allowsMetaRefresh = YES;
    _allowUniversalAccessFromFileURLs = NO;
    _treatsSHA1SignedCertificatesAsInsecure = YES;
    _needsStorageAccessFromFileURLsQuirk = YES;

    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; processPool = %@; preferences = %@>", NSStringFromClass(self.class), self, self.processPool, self.preferences];
}

// FIXME: Encode the process pool, user content controller and website data store.

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:self.processPool forKey:@"processPool"];
    [coder encodeObject:self.preferences forKey:@"preferences"];
    [coder encodeObject:self.userContentController forKey:@"userContentController"];
    [coder encodeObject:self.websiteDataStore forKey:@"websiteDataStore"];

    [coder encodeBool:self.suppressesIncrementalRendering forKey:@"suppressesIncrementalRendering"];
    [coder encodeObject:self.applicationNameForUserAgent forKey:@"applicationNameForUserAgent"];
    [coder encodeBool:self.allowsAirPlayForMediaPlayback forKey:@"allowsAirPlayForMediaPlayback"];

#if PLATFORM(IOS)
    [coder encodeInteger:self.dataDetectorTypes forKey:@"dataDetectorTypes"];
    [coder encodeBool:self.allowsInlineMediaPlayback forKey:@"allowsInlineMediaPlayback"];
    [coder encodeBool:self._allowsInlineMediaPlaybackAfterFullscreen forKey:@"allowsInlineMediaPlaybackAfterFullscreen"];
    [coder encodeBool:self.mediaTypesRequiringUserActionForPlayback forKey:@"mediaTypesRequiringUserActionForPlayback"];
    [coder encodeInteger:self.selectionGranularity forKey:@"selectionGranularity"];
    [coder encodeBool:self.allowsPictureInPictureMediaPlayback forKey:@"allowsPictureInPictureMediaPlayback"];
    [coder encodeBool:self.ignoresViewportScaleLimits forKey:@"ignoresViewportScaleLimits"];
#else
    [coder encodeInteger:self.userInterfaceDirectionPolicy forKey:@"userInterfaceDirectionPolicy"];
#endif
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    self.processPool = [coder decodeObjectForKey:@"processPool"];
    self.preferences = [coder decodeObjectForKey:@"preferences"];
    self.userContentController = [coder decodeObjectForKey:@"userContentController"];
    self.websiteDataStore = [coder decodeObjectForKey:@"websiteDataStore"];

    self.suppressesIncrementalRendering = [coder decodeBoolForKey:@"suppressesIncrementalRendering"];
    self.applicationNameForUserAgent = [coder decodeObjectForKey:@"applicationNameForUserAgent"];
    self.allowsAirPlayForMediaPlayback = [coder decodeBoolForKey:@"allowsAirPlayForMediaPlayback"];

#if PLATFORM(IOS)
    self.dataDetectorTypes = [coder decodeIntegerForKey:@"dataDetectorTypes"];
    self.allowsInlineMediaPlayback = [coder decodeBoolForKey:@"allowsInlineMediaPlayback"];
    self._allowsInlineMediaPlaybackAfterFullscreen = [coder decodeBoolForKey:@"allowsInlineMediaPlaybackAfterFullscreen"];
    self.mediaTypesRequiringUserActionForPlayback = [coder decodeBoolForKey:@"mediaTypesRequiringUserActionForPlayback"];
    self.selectionGranularity = static_cast<WKSelectionGranularity>([coder decodeIntegerForKey:@"selectionGranularity"]);
    self.allowsPictureInPictureMediaPlayback = [coder decodeBoolForKey:@"allowsPictureInPictureMediaPlayback"];
    self.ignoresViewportScaleLimits = [coder decodeBoolForKey:@"ignoresViewportScaleLimits"];
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

    configuration.processPool = self.processPool;
    configuration.preferences = self.preferences;
    configuration.userContentController = self.userContentController;
    configuration.websiteDataStore = self.websiteDataStore;
    configuration._visitedLinkStore = self._visitedLinkStore;
    configuration._relatedWebView = _relatedWebView.get().get();
    configuration._alternateWebViewForNavigationGestures = _alternateWebViewForNavigationGestures.get().get();
    configuration->_treatsSHA1SignedCertificatesAsInsecure = _treatsSHA1SignedCertificatesAsInsecure;
#if PLATFORM(IOS)
    configuration._contentProviderRegistry = self._contentProviderRegistry;
#endif

    configuration->_suppressesIncrementalRendering = self->_suppressesIncrementalRendering;
    configuration.applicationNameForUserAgent = self.applicationNameForUserAgent;

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
    configuration->_mediaTypesRequiringUserActionForPlayback = self->_mediaTypesRequiringUserActionForPlayback;
    configuration->_mainContentUserGestureOverrideEnabled = self->_mainContentUserGestureOverrideEnabled;
    configuration->_initialCapitalizationEnabled = self->_initialCapitalizationEnabled;
    configuration->_waitsForPaintAfterViewDidMoveToWindow = self->_waitsForPaintAfterViewDidMoveToWindow;
    configuration->_controlledByAutomation = self->_controlledByAutomation;

#if PLATFORM(IOS)
    configuration->_allowsInlineMediaPlayback = self->_allowsInlineMediaPlayback;
    configuration->_allowsInlineMediaPlaybackAfterFullscreen = self->_allowsInlineMediaPlaybackAfterFullscreen;
    configuration->_inlineMediaPlaybackRequiresPlaysInlineAttribute = self->_inlineMediaPlaybackRequiresPlaysInlineAttribute;
    configuration->_allowsPictureInPictureMediaPlayback = self->_allowsPictureInPictureMediaPlayback;
    configuration->_alwaysRunsAtForegroundPriority = _alwaysRunsAtForegroundPriority;
    configuration->_selectionGranularity = self->_selectionGranularity;
    configuration->_ignoresViewportScaleLimits = self->_ignoresViewportScaleLimits;
#endif
#if PLATFORM(MAC)
    configuration->_userInterfaceDirectionPolicy = self->_userInterfaceDirectionPolicy;
    configuration->_showsURLsInToolTips = self->_showsURLsInToolTips;
    configuration->_serviceControlsEnabled = self->_serviceControlsEnabled;
    configuration->_imageControlsEnabled = self->_imageControlsEnabled;
    configuration->_requiresUserActionForEditingControlsManager = self->_requiresUserActionForEditingControlsManager;
#endif
#if ENABLE(DATA_DETECTION) && PLATFORM(IOS)
    configuration->_dataDetectorTypes = self->_dataDetectorTypes;
#endif
#if ENABLE(WIRELESS_TARGET_PLAYBACK)
    configuration->_allowsAirPlayForMediaPlayback = self->_allowsAirPlayForMediaPlayback;
#endif
#if ENABLE(APPLE_PAY)
    configuration->_applePayEnabled = self->_applePayEnabled;
#endif
    configuration->_needsStorageAccessFromFileURLsQuirk = self->_needsStorageAccessFromFileURLsQuirk;
    configuration->_overrideContentSecurityPolicy = self->_overrideContentSecurityPolicy;

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

static NSString *defaultApplicationNameForUserAgent()
{
#if PLATFORM(IOS)
    return [@"Mobile/" stringByAppendingString:[UIDevice currentDevice].buildVersion];
#else
    return nil;
#endif
}

- (NSString *)applicationNameForUserAgent
{
    return _applicationNameForUserAgent.get([] { return defaultApplicationNameForUserAgent(); });
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationNameForUserAgent
{
    _applicationNameForUserAgent.set(adoptNS([applicationNameForUserAgent copy]));
}

- (_WKVisitedLinkStore *)_visitedLinkStore
{
    return _visitedLinkStore.get([] { return adoptNS([[_WKVisitedLinkStore alloc] init]); });
}

- (void)_setVisitedLinkStore:(_WKVisitedLinkStore *)visitedLinkStore
{
    _visitedLinkStore.set(visitedLinkStore);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

- (_WKWebsiteDataStore *)_websiteDataStore
{
    return self.websiteDataStore ? adoptNS([[_WKWebsiteDataStore alloc] initWithDataStore:self.websiteDataStore]).autorelease() : nullptr;
}

- (void)_setWebsiteDataStore:(_WKWebsiteDataStore *)websiteDataStore
{
    self.websiteDataStore = websiteDataStore ? websiteDataStore->_dataStore.get() : nullptr;
}

#pragma clang diagnostic pop

#if PLATFORM(IOS)
- (WKWebViewContentProviderRegistry *)_contentProviderRegistry
{
    return _contentProviderRegistry.get([] { return adoptNS([[WKWebViewContentProviderRegistry alloc] init]); });
}

- (void)_setContentProviderRegistry:(WKWebViewContentProviderRegistry *)registry
{
    _contentProviderRegistry.set(registry);
}
#endif

- (void)_validate
{
    if (!self.processPool)
        [NSException raise:NSInvalidArgumentException format:@"configuration.processPool is nil"];

    if (!self.preferences)
        [NSException raise:NSInvalidArgumentException format:@"configuration.preferences is nil"];

    if (!self.userContentController)
        [NSException raise:NSInvalidArgumentException format:@"configuration.userContentController is nil"];

    if (!self.websiteDataStore)
        [NSException raise:NSInvalidArgumentException format:@"configuration.websiteDataStore is nil"];

    if (!self._visitedLinkStore)
        [NSException raise:NSInvalidArgumentException format:@"configuration._visitedLinkStore is nil"];

#if PLATFORM(IOS)
    if (!self._contentProviderRegistry)
        [NSException raise:NSInvalidArgumentException format:@"configuration._contentProviderRegistry is nil"];
#endif
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
    return _treatsSHA1SignedCertificatesAsInsecure;
}

- (void)_setTreatsSHA1SignedCertificatesAsInsecure:(BOOL)insecure
{
    _treatsSHA1SignedCertificatesAsInsecure = insecure;
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

#if PLATFORM(IOS)
- (BOOL)_alwaysRunsAtForegroundPriority
{
    return _alwaysRunsAtForegroundPriority;
}

- (void)_setAlwaysRunsAtForegroundPriority:(BOOL)alwaysRunsAtForegroundPriority
{
    _alwaysRunsAtForegroundPriority = alwaysRunsAtForegroundPriority;
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
#endif // PLATFORM(IOS)

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
    return _initialCapitalizationEnabled;
}

- (void)_setInitialCapitalizationEnabled:(BOOL)initialCapitalizationEnabled
{
    _initialCapitalizationEnabled = initialCapitalizationEnabled;
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
    return _overrideContentSecurityPolicy;
}

- (void)_setOverrideContentSecurityPolicy:(NSString *)overrideContentSecurityPolicy
{
    _overrideContentSecurityPolicy = overrideContentSecurityPolicy;
}

@end

@implementation WKWebViewConfiguration (WKDeprecated)

#if PLATFORM(IOS)
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

#endif // PLATFORM(IOS)

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

#endif // WK_API_ENABLED
