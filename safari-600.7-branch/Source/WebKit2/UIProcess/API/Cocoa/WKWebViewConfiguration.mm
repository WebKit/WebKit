/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "WKPreferences.h"
#import "WKProcessPool.h"
#import "WKUserContentController.h"
#import "WKWebViewContentProviderRegistry.h"
#import "WeakObjCPtr.h"
#import "_WKVisitedLinkProvider.h"
#import "_WKWebsiteDataStore.h"
#import <wtf/RetainPtr.h>

template<typename T> class LazyInitialized {
public:
    template<typename F>
    T* get(F&& f)
    {
        if (!m_isInitialized) {
            m_value = f();
            m_isInitialized = true;
        }

        return m_value.get();
    }

    void set(T* t)
    {
        m_value = t;
        m_isInitialized = true;
    }

    T* peek()
    {
        return m_value.get();
    }

private:
    bool m_isInitialized = false;
    RetainPtr<T> m_value;
};

@implementation WKWebViewConfiguration {
    LazyInitialized<WKProcessPool> _processPool;
    LazyInitialized<WKPreferences> _preferences;
    LazyInitialized<WKUserContentController> _userContentController;
    LazyInitialized<_WKVisitedLinkProvider> _visitedLinkProvider;
    LazyInitialized<_WKWebsiteDataStore> _websiteDataStore;
    WebKit::WeakObjCPtr<WKWebView> _relatedWebView;
    WebKit::WeakObjCPtr<WKWebView> _alternateWebViewForNavigationGestures;
    RetainPtr<NSString> _groupIdentifier;
#if PLATFORM(IOS)
    LazyInitialized<WKWebViewContentProviderRegistry> _contentProviderRegistry;
#endif
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
#if PLATFORM(IOS)
    _mediaPlaybackRequiresUserAction = YES;
    _mediaPlaybackAllowsAirPlay = YES;
#endif
    
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; processPool = %@; preferences = %@>", NSStringFromClass(self.class), self, self.processPool, self.preferences];
}

- (id)copyWithZone:(NSZone *)zone
{
    WKWebViewConfiguration *configuration = [[[self class] allocWithZone:zone] init];

    configuration.processPool = self.processPool;
    configuration.preferences = self.preferences;
    configuration.userContentController = self.userContentController;
    configuration._visitedLinkProvider = self._visitedLinkProvider;
    configuration._websiteDataStore = self._websiteDataStore;
    configuration._relatedWebView = _relatedWebView.get().get();
    configuration._alternateWebViewForNavigationGestures = _alternateWebViewForNavigationGestures.get().get();
#if PLATFORM(IOS)
    configuration._contentProviderRegistry = self._contentProviderRegistry;
#endif

    configuration->_suppressesIncrementalRendering = self->_suppressesIncrementalRendering;
#if PLATFORM(IOS)
    configuration->_allowsInlineMediaPlayback = self->_allowsInlineMediaPlayback;
    configuration->_mediaPlaybackRequiresUserAction = self->_mediaPlaybackRequiresUserAction;
    configuration->_mediaPlaybackAllowsAirPlay = self->_mediaPlaybackAllowsAirPlay;
    configuration->_selectionGranularity = self->_selectionGranularity;
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

- (_WKVisitedLinkProvider *)_visitedLinkProvider
{
    return _visitedLinkProvider.get([] { return adoptNS([[_WKVisitedLinkProvider alloc] init]); });
}

- (void)_setVisitedLinkProvider:(_WKVisitedLinkProvider *)visitedLinkProvider
{
    _visitedLinkProvider.set(visitedLinkProvider);
}

- (_WKWebsiteDataStore *)_websiteDataStore
{
    return _websiteDataStore.get([] { return [_WKWebsiteDataStore defaultDataStore]; });
}

- (void)_setWebsiteDataStore:(_WKWebsiteDataStore *)websiteDataStore
{
    _websiteDataStore.set(websiteDataStore);
}

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

    if (!self._visitedLinkProvider)
        [NSException raise:NSInvalidArgumentException format:@"configuration._visitedLinkProvider is nil"];

    if (!self._websiteDataStore)
        [NSException raise:NSInvalidArgumentException format:@"configuration._websiteDataStore is nil"];

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

@end

#endif // WK_API_ENABLED
