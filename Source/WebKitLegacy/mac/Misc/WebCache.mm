/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebCache.h"

#import "NetworkStorageSessionMap.h"
#import "WebApplicationCacheInternal.h"
#import "WebNSObjectExtras.h"
#import "WebPreferences.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/CookieJar.h>
#import <WebCore/CredentialStorage.h>
#import <WebCore/CrossOriginPreflightResultCache.h>
#import <WebCore/Document.h>
#import <WebCore/MemoryCache.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/StorageSessionProvider.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>

#if PLATFORM(IOS_FAMILY)
#import "WebFrameInternal.h"
#import <WebCore/BackForwardCache.h>
#import <WebCore/CachedImage.h>
#import <WebCore/Frame.h>
#import <WebCore/WebCoreThreadRun.h>
#endif

class DefaultStorageSessionProvider : public WebCore::StorageSessionProvider {
    WebCore::NetworkStorageSession* storageSession() const final
    {
        return &NetworkStorageSessionMap::defaultStorageSession();
    }
};

@implementation WebCache

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    RunLoop::initializeMain();
#endif
}

+ (NSArray *)statistics
{
    auto s = WebCore::MemoryCache::singleton().getStatistics();
    return @[
        @{
            @"Images": @(s.images.count),
            @"CSS": @(s.cssStyleSheets.count),
#if ENABLE(XSLT)
            @"XSL": @(s.xslStyleSheets.count),
#else
            @"XSL": @(0),
#endif
            @"JavaScript": @(s.scripts.count),
        },
        @{
            @"Images": @(s.images.size),
            @"CSS": @(s.cssStyleSheets.size),
#if ENABLE(XSLT)
            @"XSL": @(s.xslStyleSheets.size),
#else
            @"XSL": @(0),
#endif
            @"JavaScript": @(s.scripts.size),
        },
        @{
            @"Images": @(s.images.liveSize),
            @"CSS": @(s.cssStyleSheets.liveSize),
#if ENABLE(XSLT)
            @"XSL": @(s.xslStyleSheets.liveSize),
#else
            @"XSL": @(0),
#endif
            @"JavaScript": @(s.scripts.liveSize),
        },
        @{
            @"Images": @(s.images.decodedSize),
            @"CSS": @(s.cssStyleSheets.decodedSize),
#if ENABLE(XSLT)
            @"XSL": @(s.xslStyleSheets.decodedSize),
#else
            @"XSL": @(0),
#endif
            @"JavaScript": @(s.scripts.decodedSize),
        },
    ];
}

+ (void)empty
{
    // Toggling the cache model like this forces the cache to evict all its in-memory resources.
    WebCacheModel cacheModel = [WebView _cacheModel];
    [WebView _setCacheModel:WebCacheModelDocumentViewer];
    [WebView _setCacheModel:cacheModel];

    // Empty the application cache.
    webApplicationCacheStorage().empty();

    // Empty the Cross-Origin Preflight cache
    WebCore::CrossOriginPreflightResultCache::singleton().clear();
}

#if PLATFORM(IOS_FAMILY)

+ (void)emptyInMemoryResources
{
    // This method gets called from MobileSafari after it calls [WebView
    // _close]. [WebView _close] schedules its work on the WebThread. So we
    // schedule this method on the WebThread as well so as to pick up all the
    // dead resources left behind after closing the WebViews
    WebThreadRun(^{
        // Toggling the cache model like this forces the cache to evict all its in-memory resources.
        WebCacheModel cacheModel = [WebView _cacheModel];
        [WebView _setCacheModel:WebCacheModelDocumentViewer];
        [WebView _setCacheModel:cacheModel];

        WebCore::MemoryCache::singleton().pruneLiveResources(true);
    });
}

+ (void)sizeOfDeadResources:(int *)resources
{
    WebCore::MemoryCache::Statistics stats = WebCore::MemoryCache::singleton().getStatistics();
    if (resources) {
        *resources = (stats.images.size - stats.images.liveSize)
                     + (stats.cssStyleSheets.size - stats.cssStyleSheets.liveSize)
#if ENABLE(XSLT)
                     + (stats.xslStyleSheets.size - stats.xslStyleSheets.liveSize)
#endif
                     + (stats.scripts.size - stats.scripts.liveSize);
    }
}

+ (CGImageRef)imageForURL:(NSURL *)url
{
    if (!url)
        return nullptr;
    
    WebCore::ResourceRequest request(url);
    WebCore::CachedResource* cachedResource = WebCore::MemoryCache::singleton().resourceForRequest(request, PAL::SessionID::defaultSessionID());
    if (!is<WebCore::CachedImage>(cachedResource))
        return nullptr;
    WebCore::CachedImage& cachedImage = downcast<WebCore::CachedImage>(*cachedResource);
    if (!cachedImage.hasImage())
        return nullptr;
    return cachedImage.image()->nativeImage().get();
}

#endif // PLATFORM(IOS_FAMILY)

+ (void)setDisabled:(BOOL)disabled
{
    if (!pthread_main_np())
        return [[self _webkit_invokeOnMainThread] setDisabled:disabled];

    WebCore::MemoryCache::singleton().setDisabled(disabled);
}

+ (BOOL)isDisabled
{
    return WebCore::MemoryCache::singleton().disabled();
}

+ (void)clearCachedCredentials
{
    [WebView _makeAllWebViewsPerformSelector:@selector(_clearCredentials)];
    NetworkStorageSessionMap::defaultStorageSession().credentialStorage().clearCredentials();
}

@end
