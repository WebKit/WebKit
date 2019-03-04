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

#import "config.h"
#import <WebKit/WKFoundation.h>

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKBackForwardListItemPrivate.h>
#import <WebKit/WKPage.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool didForceRepaint;

using namespace TestWebKitAPI;

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

@interface SnapshotTestWKWebView : TestWKWebView

@end

@implementation SnapshotTestWKWebView

- (instancetype)init
{
    self = [super initWithFrame:NSMakeRect(0, 0, 800, 600)];
    if (self) {
        [self _disableBackForwardSnapshotVolatilityForTesting];
        [self setAllowsBackForwardNavigationGestures:YES];
        [self _setWindowOcclusionDetectionEnabled:NO];

        [self.window orderBack:nil];
    }
    return self;
}

static void forceRepaintCallback(WKErrorRef error, void*)
{
    EXPECT_NULL(error);
    didForceRepaint = true;
}

- (void)synchronouslyForceRepaint
{
    didForceRepaint = false;
    WKPageForceRepaint([self _pageForTesting], 0, forceRepaintCallback);
    TestWebKitAPI::Util::run(&didForceRepaint);
}

- (void)synchronouslyLoadTestPageAndForceRepaint:(NSString *)testPageName
{
    [self synchronouslyLoadTestPageNamed:testPageName];
    [self synchronouslyForceRepaint];
    [[self window] display];
}

@end

static bool imagesAreEqual(CGImageRef first, CGImageRef second)
{
    RetainPtr<NSData> firstData = adoptNS((NSData *)CGDataProviderCopyData(CGImageGetDataProvider(first)));
    RetainPtr<NSData> secondData = adoptNS((NSData *)CGDataProviderCopyData(CGImageGetDataProvider(second)));

    return [firstData isEqualTo:secondData.get()];
}

TEST(SnapshotStore, SnapshotUponNavigation)
{
    RetainPtr<SnapshotTestWKWebView> webView = adoptNS([[SnapshotTestWKWebView alloc] init]);

    [webView synchronouslyLoadTestPageAndForceRepaint:@"simple"];

    RetainPtr<WKBackForwardListItem> firstItem = [[webView backForwardList] currentItem];

    RetainPtr<CGImageRef> imageBeforeNavigation = adoptCF([firstItem _copySnapshotForTesting]);
    EXPECT_NULL(imageBeforeNavigation.get());

    [webView synchronouslyLoadTestPageAndForceRepaint:@"lots-of-text"];

    // After navigating, the first item should have a valid back-forward snapshot.
    RetainPtr<CGImageRef> imageAfterNavigation = adoptCF([firstItem _copySnapshotForTesting]);
    EXPECT_NOT_NULL(imageAfterNavigation.get());
    EXPECT_EQ(CGImageGetWidth(imageAfterNavigation.get()), (unsigned long)(800 * [webView window].backingScaleFactor));
}

TEST(SnapshotStore, SnapshotClearedWhenItemIsRemoved)
{
    RetainPtr<SnapshotTestWKWebView> webView = adoptNS([[SnapshotTestWKWebView alloc] init]);

    [webView synchronouslyLoadTestPageAndForceRepaint:@"simple"];
    [webView synchronouslyLoadTestPageAndForceRepaint:@"lots-of-text"];

    RetainPtr<WKBackForwardListItem> item = [[webView backForwardList] currentItem];

    [webView synchronouslyLoadTestPageAndForceRepaint:@"simple"];

    EXPECT_NOT_NULL(adoptCF([item _copySnapshotForTesting]).get());

    [webView goBack];
    [webView _test_waitForDidFinishNavigation];
    [webView goBack];
    [webView _test_waitForDidFinishNavigation];

    // The original second item is still in the forward list, and should still have a snapshot.
    EXPECT_NOT_NULL(adoptCF([item _copySnapshotForTesting]).get());

    [webView synchronouslyLoadTestPageAndForceRepaint:@"lots-of-text"];

    // The original second item shouldn't have an image anymore, because it was invalidated
    // by navigating back past it and then doing another load.
    EXPECT_NULL(adoptCF([item _copySnapshotForTesting]).get());
}

static RetainPtr<NSView> makeRedSquareView()
{
    RetainPtr<NSBox> redSquare = adoptNS([[NSBox alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)]);
    [redSquare setBoxType:NSBoxCustom];
    [redSquare setFillColor:[NSColor redColor]];
    return redSquare;
}

TEST(SnapshotStore, ExplicitSnapshotsChangeUponNavigation)
{
    RetainPtr<SnapshotTestWKWebView> webView = adoptNS([[SnapshotTestWKWebView alloc] init]);

    RetainPtr<NSView> redSquare = makeRedSquareView();
    [[webView superview] addSubview:redSquare.get()];

    [webView synchronouslyLoadTestPageAndForceRepaint:@"lots-of-text"];

    RetainPtr<WKBackForwardListItem> firstItem = [[webView backForwardList] currentItem];
    [webView _saveBackForwardSnapshotForItem:firstItem.get()];
    [redSquare removeFromSuperview];

    RetainPtr<CGImageRef> initialSnapshot = adoptCF([firstItem _copySnapshotForTesting]);
    EXPECT_NOT_NULL(initialSnapshot);

    [webView synchronouslyLoadTestPageAndForceRepaint:@"simple"];

    // After navigating, the first item's snapshot should change.
    RetainPtr<CGImageRef> snapshotAfterNavigation = adoptCF([firstItem _copySnapshotForTesting]);
    EXPECT_NOT_NULL(snapshotAfterNavigation.get());
    EXPECT_FALSE(imagesAreEqual(initialSnapshot.get(), snapshotAfterNavigation.get()));
}

TEST(SnapshotStore, SnapshotsForNeverLoadedPagesDoNotChangeUponNavigation)
{
    RetainPtr<SnapshotTestWKWebView> webView = adoptNS([[SnapshotTestWKWebView alloc] init]);

    RetainPtr<NSView> redSquare = makeRedSquareView();
    [[webView superview] addSubview:redSquare.get()];

    [webView synchronouslyLoadTestPageAndForceRepaint:@"simple"];

    WKRetainPtr<WKSessionStateRef> sessionState = adoptWK(static_cast<WKSessionStateRef>(WKPageCopySessionState([webView _pageForTesting], nullptr, nullptr)));
    WKPageRestoreFromSessionStateWithoutNavigation([webView _pageForTesting], sessionState.get());

    RetainPtr<WKBackForwardListItem> firstItem = [[webView backForwardList] currentItem];
    [webView _saveBackForwardSnapshotForItem:firstItem.get()];
    [redSquare removeFromSuperview];

    [webView synchronouslyLoadTestPageAndForceRepaint:@"lots-of-text"];

    RetainPtr<CGImageRef> initialSnapshot = adoptCF([firstItem _copySnapshotForTesting]);
    EXPECT_NOT_NULL(initialSnapshot);

    [webView synchronouslyLoadTestPageAndForceRepaint:@"simple"];

    // After navigating, the first item's snapshot should not change, because it was
    // never loaded into the view.
    RetainPtr<CGImageRef> snapshotAfterNavigation = adoptCF([firstItem _copySnapshotForTesting]);
    EXPECT_NOT_NULL(snapshotAfterNavigation.get());
    EXPECT_TRUE(imagesAreEqual(initialSnapshot.get(), snapshotAfterNavigation.get()));
}

TEST(SnapshotStore, SnapshottingNullBackForwardItemShouldNotCrash)
{
    RetainPtr<SnapshotTestWKWebView> webView = adoptNS([[SnapshotTestWKWebView alloc] init]);
    [webView _saveBackForwardSnapshotForItem:nil];
}

#endif // PLATFORM(MAC)
