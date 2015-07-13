/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#import "WKPreferences.h"
#import "WKProcessPool.h"
#import "WKUserContentController.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WeakObjCPtr.h"
#import "_WKVisitedLinkProvider.h"
#import "_WKWebsiteDataStoreInternal.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
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
        m_value = WTF::move(t);
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
    LazyInitialized<RetainPtr<_WKVisitedLinkProvider>> _visitedLinkProvider;
    LazyInitialized<RetainPtr<WKWebsiteDataStore>> _websiteDataStore;
    WebKit::WeakObjCPtr<WKWebView> _relatedWebView;
    WebKit::WeakObjCPtr<WKWebView> _alternateWebViewForNavigationGestures;
    BOOL _treatsSHA1SignedCertificatesAsInsecure;
    RetainPtr<NSString> _groupIdentifier;
    LazyInitialized<RetainPtr<NSString>> _applicationNameForUserAgent;

#if PLATFORM(IOS)
    LazyInitialized<RetainPtr<WKWebViewContentProviderRegistry>> _contentProviderRegistry;
    BOOL _alwaysRunsAtForegroundPriority;
#endif
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
#if PLATFORM(IOS)
    _requiresUserActionForMediaPlayback = YES;
    _allowsPictureInPictureMediaPlayback = YES;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    _allowsAirPlayForMediaPlayback = YES;
#endif

    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; processPool = %@; preferences = %@>", NSStringFromClass(self.class), self, self.processPool, self.preferences];
}

- (id)copyWithZone:(NSZone *)zone
{
    WKWebViewConfiguration *configuration = [(WKWebViewConfiguration *)[[self class] allocWithZone:zone] init];

    configuration.processPool = self.processPool;
    configuration.preferences = self.preferences;
    configuration.userContentController = self.userContentController;
    configuration.websiteDataStore = self.websiteDataStore;
    configuration._visitedLinkProvider = self._visitedLinkProvider;
    configuration._relatedWebView = _relatedWebView.get().get();
    configuration._alternateWebViewForNavigationGestures = _alternateWebViewForNavigationGestures.get().get();
    configuration->_treatsSHA1SignedCertificatesAsInsecure = _treatsSHA1SignedCertificatesAsInsecure;
#if PLATFORM(IOS)
    configuration._contentProviderRegistry = self._contentProviderRegistry;
#endif

    configuration->_suppressesIncrementalRendering = self->_suppressesIncrementalRendering;
    configuration.applicationNameForUserAgent = self.applicationNameForUserAgent;

#if PLATFORM(IOS)
    configuration->_allowsInlineMediaPlayback = self->_allowsInlineMediaPlayback;
    configuration->_allowsPictureInPictureMediaPlayback = self->_allowsPictureInPictureMediaPlayback;
    configuration->_alwaysRunsAtForegroundPriority = _alwaysRunsAtForegroundPriority;
    configuration->_requiresUserActionForMediaPlayback = self->_requiresUserActionForMediaPlayback;
    configuration->_selectionGranularity = self->_selectionGranularity;
#endif
#if ENABLE(WIRELESS_TARGET_PLAYBACK)
    configuration->_allowsAirPlayForMediaPlayback = self->_allowsAirPlayForMediaPlayback;
#endif

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

- (_WKVisitedLinkProvider *)_visitedLinkProvider
{
    return _visitedLinkProvider.get([] { return adoptNS([[_WKVisitedLinkProvider alloc] init]); });
}

- (void)_setVisitedLinkProvider:(_WKVisitedLinkProvider *)visitedLinkProvider
{
    _visitedLinkProvider.set(visitedLinkProvider);
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

    if (!self._visitedLinkProvider)
        [NSException raise:NSInvalidArgumentException format:@"configuration._visitedLinkProvider is nil"];

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

#if PLATFORM(IOS)
- (BOOL)_alwaysRunsAtForegroundPriority
{
    return _alwaysRunsAtForegroundPriority;
}

- (void)_setAlwaysRunsAtForegroundPriority:(BOOL)alwaysRunsAtForegroundPriority
{
    _alwaysRunsAtForegroundPriority = alwaysRunsAtForegroundPriority;
}
#endif

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
#endif // PLATFORM(IOS)

@end

#endif // WK_API_ENABLED
