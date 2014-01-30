/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebCache.h"

#import "WebNSObjectExtras.h"
#import "WebPreferences.h"
#import "WebSystemInterface.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <WebCore/ApplicationCacheStorage.h>
#import <WebCore/CrossOriginPreflightResultCache.h>
#import <WebCore/MemoryCache.h>
#import <runtime/InitializeThreading.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>

#if PLATFORM(IOS)
#import "MemoryMeasure.h"
#import "WebFrameInternal.h"
#import <WebCore/CachedImage.h>
#import <WebCore/CredentialStorage.h>
#import <WebCore/Frame.h>
#import <WebCore/PageCache.h>
#import <WebCore/WebCoreThreadRun.h>
#endif

#if ENABLE(CACHE_PARTITIONING)
#import <WebCore/Document.h>
#endif

@implementation WebCache

+ (void)initialize
{
#if !PLATFORM(IOS)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
#endif
    InitWebCoreSystemInterface();   
}

+ (NSArray *)statistics
{
    WebCore::MemoryCache::Statistics s = WebCore::memoryCache()->getStatistics();

    return [NSArray arrayWithObjects:
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.count], @"Images",
            [NSNumber numberWithInt:s.cssStyleSheets.count], @"CSS",
#if ENABLE(XSLT)
            [NSNumber numberWithInt:s.xslStyleSheets.count], @"XSL",
#else
            [NSNumber numberWithInt:0], @"XSL",
#endif
            [NSNumber numberWithInt:s.scripts.count], @"JavaScript",
            nil],
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.size], @"Images",
            [NSNumber numberWithInt:s.cssStyleSheets.size] ,@"CSS",
#if ENABLE(XSLT)
            [NSNumber numberWithInt:s.xslStyleSheets.size], @"XSL",
#else
            [NSNumber numberWithInt:0], @"XSL",
#endif
            [NSNumber numberWithInt:s.scripts.size], @"JavaScript",
            nil],
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.liveSize], @"Images",
            [NSNumber numberWithInt:s.cssStyleSheets.liveSize] ,@"CSS",
#if ENABLE(XSLT)
            [NSNumber numberWithInt:s.xslStyleSheets.liveSize], @"XSL",
#else
            [NSNumber numberWithInt:0], @"XSL",
#endif
            [NSNumber numberWithInt:s.scripts.liveSize], @"JavaScript",
            nil],
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.decodedSize], @"Images",
            [NSNumber numberWithInt:s.cssStyleSheets.decodedSize] ,@"CSS",
#if ENABLE(XSLT)
            [NSNumber numberWithInt:s.xslStyleSheets.decodedSize], @"XSL",
#else
            [NSNumber numberWithInt:0], @"XSL",
#endif
            [NSNumber numberWithInt:s.scripts.decodedSize], @"JavaScript",
            nil],
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.purgeableSize], @"Images",
            [NSNumber numberWithInt:s.cssStyleSheets.purgeableSize] ,@"CSS",
#if ENABLE(XSLT)
            [NSNumber numberWithInt:s.xslStyleSheets.purgeableSize], @"XSL",
#else
            [NSNumber numberWithInt:0], @"XSL",
#endif
            [NSNumber numberWithInt:s.scripts.purgeableSize], @"JavaScript",
            nil],
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.purgedSize], @"Images",
            [NSNumber numberWithInt:s.cssStyleSheets.purgedSize] ,@"CSS",
#if ENABLE(XSLT)
            [NSNumber numberWithInt:s.xslStyleSheets.purgedSize], @"XSL",
#else
            [NSNumber numberWithInt:0], @"XSL",
#endif
            [NSNumber numberWithInt:s.scripts.purgedSize], @"JavaScript",
            nil],
#if ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.mappedSize], @"Images",
            [NSNumber numberWithInt:s.cssStyleSheets.mappedSize] ,@"CSS",
#if ENABLE(XSLT)
            [NSNumber numberWithInt:s.xslStyleSheets.mappedSize], @"XSL",
#else
            [NSNumber numberWithInt:0], @"XSL",
#endif
            [NSNumber numberWithInt:s.scripts.mappedSize], @"JavaScript",
            nil],
#endif // ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)
        nil];
}

+ (void)empty
{
    // Toggling the cache model like this forces the cache to evict all its in-memory resources.
    WebCacheModel cacheModel = [WebView _cacheModel];
    [WebView _setCacheModel:WebCacheModelDocumentViewer];
    [WebView _setCacheModel:cacheModel];

    // Empty the application cache.
    WebCore::cacheStorage().empty();

    // Empty the Cross-Origin Preflight cache
    WebCore::CrossOriginPreflightResultCache::shared().empty();
}

#if PLATFORM(IOS)
+ (void)emptyInMemoryResources
{
    // This method gets called from MobileSafari after it calls [WebView
    // _close]. [WebView _close] schedules its work on the WebThread. So we
    // schedule this method on the WebThread as well so as to pick up all the
    // dead resources left behind after closing the WebViews
    WebThreadRun(^{
        WebKit::MemoryMeasure measurer("[WebCache emptyInMemoryResources]");

        // Toggling the cache model like this forces the cache to evict all its in-memory resources.
        WebCacheModel cacheModel = [WebView _cacheModel];
        [WebView _setCacheModel:WebCacheModelDocumentViewer];
        [WebView _setCacheModel:cacheModel];

        WebCore::memoryCache()->pruneLiveResources(true);
    });
}

+ (void)sizeOfDeadResources:(int *)resources
{
    WebCore::MemoryCache::Statistics stats = WebCore::memoryCache()->getStatistics();
    if (resources) {
        *resources = (stats.images.size - stats.images.liveSize)
                     + (stats.cssStyleSheets.size - stats.cssStyleSheets.liveSize)
#if ENABLE(XSLT)
                     + (stats.xslStyleSheets.size - stats.xslStyleSheets.liveSize)
#endif
                     + (stats.scripts.size - stats.scripts.liveSize);
    }
}

+ (void)clearCachedCredentials
{
    WebCore::CredentialStorage::clearCredentials();
}

+ (bool)addImageToCache:(CGImageRef)image forURL:(NSURL *)url
{
    return [WebCache addImageToCache:image forURL:url forFrame:nil];
}

+ (bool)addImageToCache:(CGImageRef)image forURL:(NSURL *)url forFrame:(WebFrame *)frame
{
    if (!image || !url || ![[url absoluteString] length])
        return false;
    WebCore::SecurityOrigin* topOrigin = nullptr;
#if ENABLE(CACHE_PARTITIONING)
    if (frame)
        topOrigin = core(frame)->document()->topOrigin();
#endif
    return WebCore::memoryCache()->addImageToCache(image, url, topOrigin ? topOrigin->cachePartition() : emptyString());
}

+ (void)removeImageFromCacheForURL:(NSURL *)url
{
    [WebCache removeImageFromCacheForURL:url forFrame:nil];
}

+ (void)removeImageFromCacheForURL:(NSURL *)url forFrame:(WebFrame *)frame
{
    if (!url)
        return;
    WebCore::SecurityOrigin* topOrigin = nullptr;
#if ENABLE(CACHE_PARTITIONING)
    if (frame)
        topOrigin = core(frame)->document()->topOrigin();
#endif
    WebCore::memoryCache()->removeImageFromCache(url, topOrigin ? topOrigin->cachePartition() : emptyString());
}

+ (CGImageRef)imageForURL:(NSURL *)url
{
    if (!url)
        return nullptr;
    
    WebCore::CachedResource* cachedResource = WebCore::memoryCache()->resourceForURL(url);
    if (!cachedResource || !cachedResource->isImage())
        return nullptr;
    WebCore::CachedImage* cachedImage = static_cast<WebCore::CachedImage*>(cachedResource);
    if (!cachedImage || !cachedImage->hasImage())
        return nullptr;
    return cachedImage->image()->getCGImageRef();
}

#endif // PLATFORM(IOS)

+ (void)setDisabled:(BOOL)disabled
{
    if (!pthread_main_np())
        return [[self _webkit_invokeOnMainThread] setDisabled:disabled];

    WebCore::memoryCache()->setDisabled(disabled);
}

+ (BOOL)isDisabled
{
    return WebCore::memoryCache()->disabled();
}

@end
