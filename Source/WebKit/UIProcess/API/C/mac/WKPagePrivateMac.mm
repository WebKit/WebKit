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

#import "APIPageConfiguration.h"
#import "FullscreenClient.h"
#import "PageLoadStateObserver.h"
#import "WKAPICast.h"
#import "WKNSURLExtras.h"
#import "WKNavigationInternal.h"
#import "WKViewInternal.h"
#import "WKWebViewInternal.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessPool.h"
#import <wtf/MainThread.h>

@interface WKObservablePageState : NSObject <_WKObservablePageState> {
    @package
    RefPtr<WebKit::WebPageProxy> _page;
    RefPtr<WebKit::PageLoadStateObserver> _observer;
}

@end

static Ref<WebKit::WebPageProxy> protectedPage(WKObservablePageState *state)
{
    return *state->_page;
}

@implementation WKObservablePageState

- (id)initWithPage:(RefPtr<WebKit::WebPageProxy>&&)page
{
    if (!(self = [super init]))
        return nil;

    _page = WTF::move(page);
    Ref observer = WebKit::PageLoadStateObserver::create(self, @"URL");
    _observer = observer.get();
    protect(protectedPage(self)->pageLoadState())->addObserver(observer.get());

    return self;
}

- (void)dealloc
{
    protect(*_observer)->clearObject();

    ensureOnMainRunLoop([page = WTF::move(_page), observer = std::exchange(_observer, nullptr)] {
        protect(page->pageLoadState())->removeObserver(*observer);
    });

    [super dealloc];
}

- (BOOL)isLoading
{
    return protect(protectedPage(self)->pageLoadState())->isLoading();
}

- (NSString *)title
{
    return protect(protectedPage(self)->pageLoadState())->title().createNSString().autorelease();
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithWTFString:protect(protectedPage(self)->pageLoadState())->activeURL()];
}

- (BOOL)hasOnlySecureContent
{
    return protect(protectedPage(self)->pageLoadState())->hasOnlySecureContent();
}

- (BOOL)_webProcessIsResponsive
{
    return protect(protectedPage(self)->legacyMainFrameProcess())->isResponsive();
}

- (double)estimatedProgress
{
    return protectedPage(self)->estimatedProgress();
}

- (NSURL *)unreachableURL
{
    return [NSURL _web_URLWithWTFString:protectedPage(self)->pageLoadState().unreachableURL()];
}

- (SecTrustRef)serverTrust
{
    return protectedPage(self)->pageLoadState().certificateInfo().trust().get();
}

@end

id <_WKObservablePageState> WKPageCreateObservableState(WKPageRef pageRef)
{
    SUPPRESS_RETAINPTR_CTOR_ADOPT return [[WKObservablePageState alloc] initWithPage:WebKit::toImpl(pageRef)];
}

_WKRemoteObjectRegistry *WKPageGetObjectRegistry(WKPageRef pageRef)
{
#if PLATFORM(MAC)
    return WebKit::toProtectedImpl(pageRef)->remoteObjectRegistry();
#else
    return nil;
#endif
}

bool WKPageIsURLKnownHSTSHost(WKPageRef page, WKURLRef url)
{
    return protect(WebKit::toProtectedImpl(page)->configuration().processPool())->isURLKnownHSTSHost(WebKit::toImpl(url)->string());
}

WKNavigation *WKPageLoadURLRequestReturningNavigation(WKPageRef pageRef, WKURLRequestRef urlRequestRef)
{
    auto resourceRequest = WebKit::toImpl(urlRequestRef)->resourceRequest();
    return WebKit::wrapper(WebKit::toProtectedImpl(pageRef)->loadRequest(WTF::move(resourceRequest))).autorelease();
}

WKNavigation *WKPageLoadFileReturningNavigation(WKPageRef pageRef, WKURLRef fileURL, WKURLRef resourceDirectoryURL)
{
    return WebKit::wrapper(WebKit::toProtectedImpl(pageRef)->loadFile(WebKit::toWTFString(fileURL), WebKit::toWTFString(resourceDirectoryURL))).autorelease();
}

WKWebView *WKPageGetWebView(WKPageRef page)
{
    return page ? WebKit::toProtectedImpl(page)->cocoaView().autorelease() : nil;
}

#if PLATFORM(MAC)
bool WKPageIsPlayingVideoInEnhancedFullscreen(WKPageRef pageRef)
{
    return WebKit::toProtectedImpl(pageRef)->isPlayingVideoInEnhancedFullscreen();
}
#endif

void WKPageSetFullscreenDelegate(WKPageRef page, id <_WKFullscreenDelegate> delegate)
{
#if ENABLE(FULLSCREEN_API)
    downcast<WebKit::FullscreenClient>(WebKit::toImpl(page)->fullscreenClient()).setDelegate(delegate);
#endif
}

id <_WKFullscreenDelegate> WKPageGetFullscreenDelegate(WKPageRef page)
{
#if ENABLE(FULLSCREEN_API)
    return downcast<WebKit::FullscreenClient>(WebKit::toImpl(page)->fullscreenClient()).delegate().autorelease();
#else
    return nil;
#endif
}

NSDictionary *WKPageGetAccessibilityWebProcessDebugInfo(WKPageRef pageRef)
{
#if PLATFORM(MAC)
    return WebKit::toProtectedImpl(pageRef)->getAccessibilityWebProcessDebugInfo();
#else
    return nil;
#endif
}

NSArray *WKPageGetAccessibilityWebProcessDebugInfoForAllProcesses(WKPageRef pageRef)
{
#if PLATFORM(MAC)
    return WebKit::toProtectedImpl(pageRef)->getAccessibilityWebProcessDebugInfoForAllProcesses();
#else
    return nil;
#endif
}

void WKPageAccessibilityClearIsolatedTree(WKPageRef pageRef)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    WebKit::toProtectedImpl(pageRef)->clearAccessibilityIsolatedTree();
#endif
}
