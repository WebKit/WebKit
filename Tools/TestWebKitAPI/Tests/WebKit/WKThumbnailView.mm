/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"

#if PLATFORM(MAC)

#import "JavaScriptTest.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "TestWKWebView.h"
#import <WebKit/WKViewPrivate.h>
#import <WebKit/_WKThumbnailView.h>
#import <wtf/RetainPtr.h>

static bool didFinishLoad;
static bool didTakeSnapshot;

static void *snapshotSizeChangeKVOContext = &snapshotSizeChangeKVOContext;

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

@interface SnapshotSizeObserver : NSObject
@end

@implementation SnapshotSizeObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context == snapshotSizeChangeKVOContext) {
        didTakeSnapshot = true;
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

@end


@interface WKThumbnailViewDelegate : NSObject <WKNavigationDelegate>
@end

@implementation WKThumbnailViewDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

#if WK_HAVE_C_SPI
    
static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

TEST(WebKit, WKThumbnailViewKeepSnapshotWhenRemovedFromSuperview)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    WKView *wkView = webView.platformView();
    setPageLoaderClient(webView.page());
    WKPageSetCustomBackingScaleFactor(webView.page(), 1);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("lots-of-text", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    RetainPtr<_WKThumbnailView> thumbnailView = adoptNS([[_WKThumbnailView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) fromWKView:wkView]);

    RetainPtr<SnapshotSizeObserver> observer = adoptNS([[SnapshotSizeObserver alloc] init]);

    [thumbnailView addObserver:observer.get() forKeyPath:@"snapshotSize" options:NSKeyValueObservingOptionNew context:snapshotSizeChangeKVOContext];

    [wkView.window.contentView addSubview:thumbnailView.get()];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    EXPECT_EQ([thumbnailView snapshotSize].width, 800);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    [thumbnailView removeFromSuperview];

    // The snapshot should be removed when unparented.
    EXPECT_TRUE([thumbnailView layer].contents == nil);

    [wkView.window.contentView addSubview:thumbnailView.get()];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    [thumbnailView setShouldKeepSnapshotWhenRemovedFromSuperview:YES];

    EXPECT_EQ([thumbnailView snapshotSize].width, 800);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    [thumbnailView removeFromSuperview];

    // This time, the snapshot should remain while unparented, because we unset shouldKeepSnapshotWhenRemovedFromSuperview.
    EXPECT_FALSE([thumbnailView layer].contents == nil);

    [thumbnailView removeObserver:observer.get() forKeyPath:@"snapshotSize" context:snapshotSizeChangeKVOContext];
}

TEST(WebKit, WKThumbnailViewMaximumSnapshotSize)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    WKView *wkView = webView.platformView();
    setPageLoaderClient(webView.page());
    WKPageSetCustomBackingScaleFactor(webView.page(), 1);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("lots-of-text", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    RetainPtr<_WKThumbnailView> thumbnailView = adoptNS([[_WKThumbnailView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) fromWKView:wkView]);

    RetainPtr<SnapshotSizeObserver> observer = adoptNS([[SnapshotSizeObserver alloc] init]);

    [thumbnailView addObserver:observer.get() forKeyPath:@"snapshotSize" options:NSKeyValueObservingOptionNew context:snapshotSizeChangeKVOContext];

    [wkView.window.contentView addSubview:thumbnailView.get()];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    EXPECT_EQ([thumbnailView snapshotSize].width, 800);
    EXPECT_FALSE([thumbnailView layer].contents == nil);

    [thumbnailView setMaximumSnapshotSize:CGSizeMake(200, 0)];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    EXPECT_EQ([thumbnailView snapshotSize].width, 200);
    EXPECT_EQ([thumbnailView snapshotSize].height, 150);
    EXPECT_FALSE([thumbnailView layer].contents == nil);

    [thumbnailView setMaximumSnapshotSize:CGSizeMake(0, 300)];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    EXPECT_EQ([thumbnailView snapshotSize].width, 400);
    EXPECT_EQ([thumbnailView snapshotSize].height, 300);
    EXPECT_FALSE([thumbnailView layer].contents == nil);

    [thumbnailView setMaximumSnapshotSize:CGSizeMake(200, 300)];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    EXPECT_EQ([thumbnailView snapshotSize].width, 200);
    EXPECT_EQ([thumbnailView snapshotSize].height, 150);
    EXPECT_FALSE([thumbnailView layer].contents == nil);

    [thumbnailView removeObserver:observer.get() forKeyPath:@"snapshotSize" context:snapshotSizeChangeKVOContext];
}

#endif // WK_HAVE_C_SPI

TEST(WebKit, WKThumbnailViewKeepSnapshotWhenRemovedFromSuperviewWKWebView)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    WKPageSetCustomBackingScaleFactor([webView _pageForTesting], 1);
    auto delegate = adoptNS([[WKThumbnailViewDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView  loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);

    RetainPtr<_WKThumbnailView> thumbnailView = adoptNS([[_WKThumbnailView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) fromWKWebView:webView.get()]);
    
    RetainPtr<SnapshotSizeObserver> observer = adoptNS([[SnapshotSizeObserver alloc] init]);
    
    [thumbnailView addObserver:observer.get() forKeyPath:@"snapshotSize" options:NSKeyValueObservingOptionNew context:snapshotSizeChangeKVOContext];
    
    [webView addSubview:thumbnailView.get()];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;
    
    EXPECT_EQ([thumbnailView snapshotSize].width, 800);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    [thumbnailView removeFromSuperview];
    
    // The snapshot should be removed when unparented.
    EXPECT_TRUE([thumbnailView layer].contents == nil);
    
    [webView addSubview:thumbnailView.get()];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;
    
    [thumbnailView setShouldKeepSnapshotWhenRemovedFromSuperview:YES];
    
    EXPECT_EQ([thumbnailView snapshotSize].width, 800);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    [thumbnailView removeFromSuperview];
    
    // This time, the snapshot should remain while unparented, because we unset shouldKeepSnapshotWhenRemovedFromSuperview.
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    
    [thumbnailView removeObserver:observer.get() forKeyPath:@"snapshotSize" context:snapshotSizeChangeKVOContext];
}

TEST(WebKit, WKThumbnailViewMaximumSnapshotSizeWKWebView)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    WKPageSetCustomBackingScaleFactor([webView _pageForTesting], 1);
    auto delegate = adoptNS([[WKThumbnailViewDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView  loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);

    auto thumbnailView = adoptNS([[_WKThumbnailView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) fromWKWebView:webView.get()]);

    auto observer = adoptNS([[SnapshotSizeObserver alloc] init]);
    
    [thumbnailView addObserver:observer.get() forKeyPath:@"snapshotSize" options:NSKeyValueObservingOptionNew context:snapshotSizeChangeKVOContext];
    
    [webView addSubview:thumbnailView.get()];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;
    EXPECT_EQ([thumbnailView snapshotSize].width, 800);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    
    [thumbnailView setMaximumSnapshotSize:CGSizeMake(200, 0)];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;
    
    EXPECT_EQ([thumbnailView snapshotSize].width, 200);
    EXPECT_EQ([thumbnailView snapshotSize].height, 150);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    
    [thumbnailView setMaximumSnapshotSize:CGSizeMake(0, 300)];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;
    
    EXPECT_EQ([thumbnailView snapshotSize].width, 400);
    EXPECT_EQ([thumbnailView snapshotSize].height, 300);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    
    [thumbnailView setMaximumSnapshotSize:CGSizeMake(200, 300)];
    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;
    
    EXPECT_EQ([thumbnailView snapshotSize].width, 200);
    EXPECT_EQ([thumbnailView snapshotSize].height, 150);
    EXPECT_FALSE([thumbnailView layer].contents == nil);
    
    [thumbnailView removeObserver:observer.get() forKeyPath:@"snapshotSize" context:snapshotSizeChangeKVOContext];
}

} // namespace TestWebKitAPI

#endif
