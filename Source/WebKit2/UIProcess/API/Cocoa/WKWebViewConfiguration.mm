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

#import "WKWebViewContentProviderRegistry.h"
#import "WeakObjCPtr.h"
#import <wtf/RetainPtr.h>

@implementation WKWebViewConfiguration {
    RetainPtr<WKProcessPool> _processPool;
    RetainPtr<WKPreferences> _preferences;
    RetainPtr<WKVisitedLinkProvider> _visitedLinkProvider;
    WebKit::WeakObjCPtr<WKWebView> _relatedWebView;
    RetainPtr<NSString> _groupIdentifier;
#if PLATFORM(IOS)
    RetainPtr<WKWebViewContentProviderRegistry> _contentProviderRegistry;
#endif
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; processPool = %@; preferences = %@>", NSStringFromClass(self.class), self, _processPool.get(), _preferences.get()];
}

- (id)copyWithZone:(NSZone *)zone
{
    WKWebViewConfiguration *configuration = [[[self class] allocWithZone:zone] init];

    configuration.processPool = _processPool.get();
    configuration.preferences = _preferences.get();
    configuration._relatedWebView = _relatedWebView.get().get();
#if PLATFORM(IOS)
    configuration._contentProviderRegistry = _contentProviderRegistry.get();
#endif

    return configuration;
}

- (WKProcessPool *)processPool
{
    return _processPool.get();
}

- (void)setProcessPool:(WKProcessPool *)processPool
{
    _processPool = processPool;
}

- (WKPreferences *)preferences
{
    return _preferences.get();
}

- (void)setPreferences:(WKPreferences *)preferences
{
    _preferences = preferences;
}

- (WKVisitedLinkProvider *)visitedLinkProvider
{
    return _visitedLinkProvider.get();
}

- (void)setVisitedLinkProvider:(WKVisitedLinkProvider *)visitedLinkProvider
{
    _visitedLinkProvider = visitedLinkProvider;
}

#if PLATFORM(IOS)
- (WKWebViewContentProviderRegistry *)_contentProviderRegistry
{
    return _contentProviderRegistry.get();
}

- (void)_setContentProviderRegistry:(WKWebViewContentProviderRegistry *)registry
{
    _contentProviderRegistry = registry;
}
#endif

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
