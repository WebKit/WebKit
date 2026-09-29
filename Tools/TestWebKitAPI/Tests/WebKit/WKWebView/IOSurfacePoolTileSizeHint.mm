/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/UISideCompositingScope.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSScrollerImpSPI.h>
#endif

// The UI process estimates the bytes in one main frame tile from the view's width and the device scale factor
// (width x scale by 512 x scale, 4 bytes per pixel) and sends it to the GPU process, which sizes the web process's
// IOSurfacePool in-use limit from it. These tests check that the estimate matches the real tiles when the page
// scrolls only vertically, and that it stays the view's estimate, with the limit still safe, when the real tiles
// differ.

// Its own namespace, so its helpers can't collide with those of other files in the same unified source bundle.
namespace TestWebKitAPI::IOSurfacePoolTileSizeHintTests {

// Each page lays out at the view's width with a page scale of 1.
static NSString *const tallPage = @"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='margin: 0'><div style='height: 5000px'></div></body>";
static NSString *const shortPage = @"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='margin: 0'><div style='height: 100px'></div></body>";
static NSString *const widePage = @"<meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'><body style='margin: 0'><div style='width: 3000px; height: 5000px'></div></body>";

// The in-use limit is 8 x the hint (front and back buffers for 4 tiles), at least the platform default and at most
// half the 256 MB pool. The macOS default already is half the pool, so there the hint never changes the limit.
#if PLATFORM(MAC)
static constexpr uint64_t defaultInUseBytesLimit = 128 * MB;
#else
static constexpr uint64_t defaultInUseBytesLimit = 32 * MB;
#endif

static uint64_t expectedInUseBytesLimit(uint64_t limitOnIOS)
{
#if PLATFORM(MAC)
    UNUSED_PARAM(limitOnIOS);
    return defaultInUseBytesLimit;
#else
    return limitOnIOS;
#endif
}

// TestWebKitAPI runs macOS with classic scrollbars, which take their width out of a page that scrolls. The hint is
// computed from the view's width, so there it slightly overestimates the tiles of a page that scrolls vertically.
static CGFloat scrollbarWidth()
{
#if PLATFORM(MAC)
    return [[NSScrollerImp self] scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScroller.preferredScrollerStyle];
#else
    return 0;
#endif
}

struct IOSurfacePoolState {
    uint64_t tileSizeHint { 0 };
    uint64_t inUseBytesLimit { 0 };
};

static IOSurfacePoolState ioSurfacePoolState(WKWebView *webView)
{
    __block bool done = false;
    __block IOSurfacePoolState state;
    [webView _ioSurfacePoolStateForTesting:^(uint64_t tileSizeHint, uint64_t inUseBytesLimit) {
        state = { tileSizeHint, inUseBytesLimit };
        done = true;
    }];
    Util::run(&done);
    return state;
}

// Checks this page's hint in the UI process, the hint the GPU process holds for its web process, and the in-use
// limit the GPU process derives from it.
static void expectHint(WKWebView *webView, uint64_t expectedHint, uint64_t expectedInUseBytesLimitOnIOS)
{
    EXPECT_EQ(webView._ioSurfacePoolTileSizeHintForTesting, expectedHint);
    auto state = ioSurfacePoolState(webView);
    EXPECT_EQ(state.tileSizeHint, expectedHint);
    EXPECT_EQ(state.inUseBytesLimit, expectedInUseBytesLimit(expectedInUseBytesLimitOnIOS));
}

// The size of a main frame tile's backing surface in pixels: its size in points times the device scale factor.
static CGSize mainFrameTilePixelSize(WKWebView *webView)
{
    __block bool done = false;
    __block CGSize result = CGSizeZero;
    [webView _mainFrameTileSizeForTesting:^(CGSize tileSize) {
        result = tileSize;
        done = true;
    }];
    Util::run(&done);
    return result;
}

// Waits for the main frame's tile surfaces to reach the expected size in pixels. TileController applies a new tile
// size 500 ms after the change that calls for it.
static void expectMainFrameTilePixelSize(TestWKWebView *webView, CGSize expected)
{
    CGSize tileSize = CGSizeZero;
    Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        tileSize = mainFrameTilePixelSize(webView);
        return CGSizeEqualToSize(tileSize, expected);
    }, 30);
    EXPECT_EQ(tileSize.width, expected.width);
    EXPECT_EQ(tileSize.height, expected.height);
}

static void resizeWebView(TestWKWebView *webView, CGFloat width, CGFloat height)
{
#if PLATFORM(MAC)
    // Keep the view inside its window.
    [[webView window] setFrame:NSMakeRect(0, 0, width, height) display:YES];
#endif
    [webView setFrame:CGRectMake(0, 0, width, height)];
}

class IOSurfacePoolTileSizeHint : public testing::Test {
public:
    // The GPU process only has a pool for a web process that renders through it. On macOS that takes UI-side
    // compositing (the default on iOS) and DOM rendering in the GPU process.
    RetainPtr<TestWKWebView> createWebView(CGFloat width, CGFloat height, CGFloat deviceScaleFactor, WKWebViewConfiguration *configuration = nil)
    {
        RetainPtr<WKWebViewConfiguration> webViewConfiguration = configuration;
        if (!webViewConfiguration)
            webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[webViewConfiguration preferences], true, adoptWK(WKStringCreateWithUTF8CString("UseGPUProcessForDOMRenderingEnabled")).get());

        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, width, height) configuration:webViewConfiguration.get()]);
        [webView _setOverrideDeviceScaleFactor:deviceScaleFactor];
#if PLATFORM(MAC)
        // An inactive window gets square tiles; see InactiveWindowOverestimates.
        [[webView window] makeKeyWindow];
#endif
        return webView;
    }

private:
#if PLATFORM(MAC)
    UISideCompositingScope m_uiSideCompositingScope { UISideCompositingState::Enabled };
#endif
};

TEST_F(IOSurfacePoolTileSizeHint, MatchesMainFrameTile)
{
    auto webView = createWebView(800, 600, 2);
    [webView synchronouslyLoadHTMLString:tallPage];

    // A page that scrolls only vertically gets tiles as wide as the page and 512 points tall, which at 2x is
    // 1600 x 1024 pixels (less the scrollbar on macOS).
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(2 * (800 - scrollbarWidth()), 2 * 512));
    // One 1600 x 1024 pixel tile at 4 bytes per pixel is 6.25 MB; 8 of them is 50 MB.
    expectHint(webView.get(), 6553600, 50 * MB);
}

TEST_F(IOSurfacePoolTileSizeHint, FollowsViewResize)
{
    auto webView = createWebView(800, 600, 3);
    [webView synchronouslyLoadHTMLString:tallPage];
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(3 * (800 - scrollbarWidth()), 3 * 512));
    // One 2400 x 1536 pixel tile is 14.0625 MB; 8 of them is 112.5 MB.
    expectHint(webView.get(), 14745600, 117964800);

    resizeWebView(webView.get(), 1200, 600);
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(3 * (1200 - scrollbarWidth()), 3 * 512));
    // One 3600 x 1536 pixel tile is 21.09375 MB; 8 of them would be more than half the pool.
    expectHint(webView.get(), 22118400, 128 * MB);
}

TEST_F(IOSurfacePoolTileSizeHint, FollowsDeviceScaleFactor)
{
    auto webView = createWebView(800, 600, 1);
    [webView synchronouslyLoadHTMLString:tallPage];
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(1 * (800 - scrollbarWidth()), 1 * 512));
    // One 800 x 512 pixel tile is 1.5625 MB; 8 of them is less than the platform default.
    expectHint(webView.get(), 1638400, 32 * MB);

    [webView _setOverrideDeviceScaleFactor:3];
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(3 * (800 - scrollbarWidth()), 3 * 512));
    // One 2400 x 1536 pixel tile is 14.0625 MB; 8 of them is 112.5 MB.
    expectHint(webView.get(), 14745600, 117964800);
}

TEST_F(IOSurfacePoolTileSizeHint, NewGPUProcessGetsCurrentHint)
{
    auto webView = createWebView(800, 600, 2);
    [webView synchronouslyLoadHTMLString:tallPage];
    expectHint(webView.get(), 6553600, 50 * MB);

    WKProcessPool *processPool = [webView configuration].processPool;
    pid_t initialGPUProcessIdentifier = [processPool _gpuProcessIdentifier];
    ASSERT_NE(initialGPUProcessIdentifier, 0);
    kill(initialGPUProcessIdentifier, SIGKILL);

    // Repaint until the web process is connected to a new GPU process. Nothing about the view changes, so the new
    // connection can only have the hint from the parameters it was created with.
    bool reconnected = Util::waitFor([&] {
        [webView stringByEvaluatingJavaScript:@"document.body.style.backgroundColor = document.body.style.backgroundColor == 'green' ? 'blue' : 'green'"];
        [webView waitForNextPresentationUpdate];
        pid_t gpuProcessIdentifier = [processPool _gpuProcessIdentifier];
        return gpuProcessIdentifier && gpuProcessIdentifier != initialGPUProcessIdentifier && ioSurfacePoolState(webView.get()).tileSizeHint;
    });
    ASSERT_TRUE(reconnected);
    expectHint(webView.get(), 6553600, 50 * MB);
}

TEST_F(IOSurfacePoolTileSizeHint, LargestPageInProcessWins)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].javaScriptCanOpenWindowsAutomatically = YES;
    auto opener = createWebView(800, 600, 2, configuration.get());

    __block RetainPtr<TestWKWebView> popup;
    __block bool popupCreated = false;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [uiDelegate setCreateWebViewWithConfiguration:^WKWebView *(WKWebViewConfiguration *popupConfiguration, WKNavigationAction *, WKWindowFeatures *) {
        popup = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 1200, 600) configuration:popupConfiguration]);
        [popup _setOverrideDeviceScaleFactor:2];
        popupCreated = true;
        return popup.get();
    }];
    [opener setUIDelegate:uiDelegate.get()];
    [opener synchronouslyLoadHTMLString:tallPage];

    [opener evaluateJavaScript:@"window.open('about:blank')" completionHandler:nil];
    Util::run(&popupCreated);
    // The popup shares the opener's web process, so the GPU process holds one hint for both pages.
    ASSERT_EQ([popup _webProcessIdentifier], [opener _webProcessIdentifier]);
    [popup waitForNextPresentationUpdate];

    EXPECT_EQ([opener _ioSurfacePoolTileSizeHintForTesting], 6553600u);
    EXPECT_EQ([popup _ioSurfacePoolTileSizeHintForTesting], 9830400u);
    // One of the popup's 2400 x 1024 pixel tiles is 9.375 MB; 8 of them is 75 MB.
    auto state = ioSurfacePoolState(opener.get());
    EXPECT_EQ(state.tileSizeHint, 9830400u);
    EXPECT_EQ(state.inUseBytesLimit, expectedInUseBytesLimit(75 * MB));

    // Closing the larger page leaves the opener's hint.
    [popup _close];
    popup = nil;
    Util::waitFor([&] {
        return ioSurfacePoolState(opener.get()).tileSizeHint == 6553600;
    });
    expectHint(opener.get(), 6553600, 50 * MB);
}

TEST_F(IOSurfacePoolTileSizeHint, ZoomedInOverestimates)
{
    auto webView = createWebView(800, 600, 2);
    [webView synchronouslyLoadHTMLString:tallPage];
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(2 * (800 - scrollbarWidth()), 2 * 512));

#if PLATFORM(MAC)
    [webView _setPageScale:2 withOrigin:CGPointZero];
#else
    [[webView scrollView] setZoomScale:2 animated:NO];
#endif

    // Zoomed in, the page also scrolls horizontally, so its tiles are 512 x 512 points, 1024 x 1024 pixels at 2x.
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(2 * 512, 2 * 512));
    // The hint is still the view's, larger than the real tiles, and the limit covers 8 real tiles of 4 MB each.
    expectHint(webView.get(), 6553600, 50 * MB);
    EXPECT_GE(ioSurfacePoolState(webView.get()).inUseBytesLimit, 8 * 4 * MB);
}

TEST_F(IOSurfacePoolTileSizeHint, HorizontallyScrollingPageOverestimates)
{
    auto webView = createWebView(800, 600, 2);
    [webView synchronouslyLoadHTMLString:widePage];

    // A page that also scrolls horizontally gets 512 x 512 point tiles, 1024 x 1024 pixels at 2x.
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(2 * 512, 2 * 512));
    // The page's width doesn't move the hint. It's still the view's, larger than the real tiles, and the limit
    // covers 8 real tiles of 4 MB each.
    expectHint(webView.get(), 6553600, 50 * MB);
    EXPECT_GE(ioSurfacePoolState(webView.get()).inUseBytesLimit, 8 * 4 * MB);
}

#if PLATFORM(MAC)
TEST_F(IOSurfacePoolTileSizeHint, InactiveWindowOverestimates)
{
    auto webView = createWebView(800, 600, 2);
    [webView synchronouslyLoadHTMLString:tallPage];
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(2 * (800 - scrollbarWidth()), 2 * 512));

    // An inactive window gets 512 x 512 point tiles, 1024 x 1024 pixels at 2x, to help app nap.
    [[webView window] resignKeyWindow];
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(2 * 512, 2 * 512));
    // The hint is still the view's, larger than the real tiles, and the limit covers 8 real tiles of 4 MB each.
    expectHint(webView.get(), 6553600, 50 * MB);
    EXPECT_GE(ioSurfacePoolState(webView.get()).inUseBytesLimit, 8 * 4 * MB);
}
#endif

TEST_F(IOSurfacePoolTileSizeHint, ShortPageUnderestimates)
{
    auto webView = createWebView(800, 600, 2);
    [webView synchronouslyLoadHTMLString:shortPage];

    // A page that doesn't scroll gets one tile the size of the view, 800 x 600 points, taller than 512 points.
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(2 * 800, 2 * 600));
    // The hint is still the view's, smaller than the real tile, and the limit never drops below the default.
    expectHint(webView.get(), 6553600, 50 * MB);
    EXPECT_GE(ioSurfacePoolState(webView.get()).inUseBytesLimit, defaultInUseBytesLimit);
}

TEST_F(IOSurfacePoolTileSizeHint, GiantTilesUnderestimate)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetBoolValueForKeyForTesting((__bridge WKPreferencesRef)[configuration preferences], true, adoptWK(WKStringCreateWithUTF8CString("UseGiantTiles")).get());
    // At 1x, so that a giant tile is 64 MB rather than 256 MB.
    auto webView = createWebView(800, 600, 1, configuration.get());
    [webView synchronouslyLoadHTMLString:tallPage];

    // Giant tiles are 4096 x 4096 points, which at 1x is the same in pixels.
    expectMainFrameTilePixelSize(webView.get(), CGSizeMake(1 * 4096, 1 * 4096));
    // The hint is still the view's 1.5625 MB, far smaller than the real tiles, and the limit stays at the default.
    expectHint(webView.get(), 1638400, 32 * MB);
    EXPECT_GE(ioSurfacePoolState(webView.get()).inUseBytesLimit, defaultInUseBytesLimit);
}

} // namespace TestWebKitAPI::IOSurfacePoolTileSizeHintTests

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
