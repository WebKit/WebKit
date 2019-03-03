/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "WKPagePrivateMac.h"

#import "FullscreenClient.h"
#import "PageLoadStateObserver.h"
#import "WKAPICast.h"
#import "WKNSURLExtras.h"
#import "WKNavigationInternal.h"
#import "WKViewInternal.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessPool.h"

@interface WKObservablePageState : NSObject <_WKObservablePageState> {
    RefPtr<WebKit::WebPageProxy> _page;
    std::unique_ptr<WebKit::PageLoadStateObserver> _observer;
}

@end

@implementation WKObservablePageState

- (id)initWithPage:(RefPtr<WebKit::WebPageProxy>&&)page
{
    if (!(self = [super init]))
        return nil;

    _page = WTFMove(page);
    _observer = std::make_unique<WebKit::PageLoadStateObserver>(self, @"URL");
    _page->pageLoadState().addObserver(*_observer);

    return self;
}

- (void)dealloc
{
    _page->pageLoadState().removeObserver(*_observer);

    [super dealloc];
}

- (BOOL)isLoading
{
    return _page->pageLoadState().isLoading();
}

- (NSString *)title
{
    return _page->pageLoadState().title();
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().activeURL()];
}

- (BOOL)hasOnlySecureContent
{
    return _page->pageLoadState().hasOnlySecureContent();
}

- (BOOL)_webProcessIsResponsive
{
    return _page->process().isResponsive();
}

- (double)estimatedProgress
{
    return _page->estimatedProgress();
}

- (NSURL *)unreachableURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().unreachableURL()];
}

- (SecTrustRef)serverTrust
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    auto certificateInfo = _page->pageLoadState().certificateInfo();
    if (!certificateInfo)
        return nil;

    return certificateInfo->certificateInfo().trust();
#else
    return nil;
#endif
}

@end

id <_WKObservablePageState> WKPageCreateObservableState(WKPageRef pageRef)
{
    return [[WKObservablePageState alloc] initWithPage:WebKit::toImpl(pageRef)];
}

_WKRemoteObjectRegistry *WKPageGetObjectRegistry(WKPageRef pageRef)
{
#if WK_API_ENABLED && !TARGET_OS_IPHONE
    return WebKit::toImpl(pageRef)->remoteObjectRegistry();
#else
    return nil;
#endif
}

bool WKPageIsURLKnownHSTSHost(WKPageRef page, WKURLRef url)
{
    WebKit::WebPageProxy* webPageProxy = WebKit::toImpl(page);
    bool privateBrowsingEnabled = webPageProxy->pageGroup().preferences().privateBrowsingEnabled();

    return webPageProxy->process().processPool().isURLKnownHSTSHost(WebKit::toImpl(url)->string(), privateBrowsingEnabled);
}

WKNavigation *WKPageLoadURLRequestReturningNavigation(WKPageRef pageRef, WKURLRequestRef urlRequestRef)
{
    auto resourceRequest = WebKit::toImpl(urlRequestRef)->resourceRequest();
    return WebKit::wrapper(WebKit::toImpl(pageRef)->loadRequest(WTFMove(resourceRequest)));
}

WKNavigation *WKPageLoadFileReturningNavigation(WKPageRef pageRef, WKURLRef fileURL, WKURLRef resourceDirectoryURL)
{
    return WebKit::wrapper(WebKit::toImpl(pageRef)->loadFile(WebKit::toWTFString(fileURL), WebKit::toWTFString(resourceDirectoryURL)));
}

#if PLATFORM(MAC)
bool WKPageIsPlayingVideoInEnhancedFullscreen(WKPageRef pageRef)
{
    return WebKit::toImpl(pageRef)->isPlayingVideoInEnhancedFullscreen();
}
#endif

void WKPageSetFullscreenDelegate(WKPageRef page, id <_WKFullscreenDelegate> delegate)
{
#if WK_API_ENABLED && ENABLE(FULLSCREEN_API)
    downcast<WebKit::FullscreenClient>(WebKit::toImpl(page)->fullscreenClient()).setDelegate(delegate);
#endif
}

id <_WKFullscreenDelegate> WKPageGetFullscreenDelegate(WKPageRef page)
{
#if WK_API_ENABLED && ENABLE(FULLSCREEN_API)
    return downcast<WebKit::FullscreenClient>(WebKit::toImpl(page)->fullscreenClient()).delegate().autorelease();
#else
    return nil;
#endif
}

