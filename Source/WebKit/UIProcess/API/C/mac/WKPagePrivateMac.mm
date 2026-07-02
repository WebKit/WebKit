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

WKNavigation *WKPageLoadFileReturningNavigation(WKPageRef pageRef, WKURLRef fileURL, WKURLRef resourceDirectoryURL)
{
    return WebKit::wrapper(protect(WebKit::toImpl(pageRef))->loadFile(WebKit::toWTFString(fileURL), WebKit::toWTFString(resourceDirectoryURL))).autorelease();
}

WKWebView *WKPageGetWebView(WKPageRef page)
{
    return page ? protect(WebKit::toImpl(page))->cocoaView().autorelease() : nil;
}

#if PLATFORM(MAC)
bool WKPageIsPlayingVideoInPictureInPicture(WKPageRef pageRef)
{
    return protect(WebKit::toImpl(pageRef))->isPlayingVideoInPictureInPicture();
}

bool WKPageIsPlayingVideoInEnhancedFullscreen(WKPageRef pageRef)
{
    return WKPageIsPlayingVideoInPictureInPicture(pageRef);
}
#endif

NSDictionary *WKPageGetAccessibilityWebProcessDebugInfo(WKPageRef pageRef)
{
#if PLATFORM(MAC)
    return protect(WebKit::toImpl(pageRef))->getAccessibilityWebProcessDebugInfo();
#else
    return nil;
#endif
}

NSArray *WKPageGetAccessibilityWebProcessDebugInfoForAllProcesses(WKPageRef pageRef)
{
#if PLATFORM(MAC)
    return protect(WebKit::toImpl(pageRef))->getAccessibilityWebProcessDebugInfoForAllProcesses();
#else
    return nil;
#endif
}

void WKPageAccessibilityClearIsolatedTree(WKPageRef pageRef)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    protect(WebKit::toImpl(pageRef))->clearAccessibilityIsolatedTree();
#endif
}
