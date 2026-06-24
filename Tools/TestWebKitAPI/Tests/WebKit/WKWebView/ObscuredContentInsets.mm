/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "Helpers/mac/AppKitSPI.h"
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestCocoaImageAndCocoaColor.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebCore/ColorCocoa.h>
#import <WebCore/ColorSerialization.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

@interface TopScrollPocketObserver : NSObject
@property (nonatomic, readonly) NSUInteger changeCount;
- (instancetype)initWithWebView:(WKWebView *)webView;
@end

@implementation TopScrollPocketObserver {
    RetainPtr<WKWebView> _webView;
    NSUInteger _changeCount;
}

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (self = [super init]) {
        _webView = webView;
        [_webView addObserver:self forKeyPath:@"_topScrollPocket" options:NSKeyValueObservingOptionNew context:nil];
    }
    return self;
}

- (void)dealloc
{
    if (_webView) {
        [_webView removeObserver:self forKeyPath:@"_topScrollPocket"];
        _webView = nil;
    }

    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if ([keyPath isEqualToString:@"_topScrollPocket"])
        _changeCount++;
}

- (NSUInteger)changeCount
{
    return _changeCount;
}

@end

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)

@interface WKWebView (ObscuredContentInsets) <NSScrollViewSeparatorTrackingAdapter>
@end

@interface FullscreenChangeMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation FullscreenChangeMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    NSString *bodyString = (NSString *)[message body];
    if ([bodyString isEqualToString:@"fullscreenchange"])
        receivedFullscreenChangeMessage = true;
    else if ([bodyString isEqualToString:@"load"])
        receivedLoadedMessage = true;
}
@end

namespace TestWebKitAPI {

TEST(TopContentInset, Fullscreen)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr<FullscreenChangeMessageHandler> handler = adoptNS([[FullscreenChangeMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"fullscreenChangeHandler"];
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView _setTopContentInset:10];
    [webView _setAutomaticallyAdjustsContentInsets:NO];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"FullscreenTopContentInset" withExtension:@"html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedLoadedMessage);

    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(5, 5) modifierFlags:0 timestamp:0 windowNumber:window.get().windowNumber context:0 eventNumber:0 clickCount:0 pressure:0];
    [webView mouseDown:event];

    TestWebKitAPI::Util::run(&receivedFullscreenChangeMessage);
    ASSERT_EQ(window.get().screen.frame.size.width, webView.get().frame.size.width);
    ASSERT_EQ(window.get().screen.frame.size.height + webView.get()._topContentInset, webView.get().frame.size.height);

    receivedFullscreenChangeMessage = false;
    [webView mouseDown:event];
    TestWebKitAPI::Util::run(&receivedFullscreenChangeMessage);
    ASSERT_EQ(10, webView.get()._topContentInset);
}

TEST(TopContentInset, AutomaticAdjustment)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    [webView loadHTMLString:@"" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    ASSERT_GT(webView.get()._topContentInset, 10);
}

TEST(TopContentInset, AutomaticAdjustmentDisabled)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView _setAutomaticallyAdjustsContentInsets:NO];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    [webView loadHTMLString:@"" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    ASSERT_EQ(webView.get()._topContentInset, 0);
}

TEST(TopContentInset, AutomaticAdjustmentDoesNotAffectInsetViews)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(100, 100, 200, 200) configuration:configuration.get()]);

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    [webView loadHTMLString:@"" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    ASSERT_EQ(webView.get()._topContentInset, 0);
}

TEST(ObscuredContentInsets, SetAndGetObscuredContentInsets)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setAutomaticallyAdjustsContentInsets:NO];

    auto initialInsets = NSEdgeInsetsMake(100, 150, 0, 10);
    [webView setObscuredContentInsets:initialInsets];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    EXPECT_TRUE(NSEdgeInsetsEqual([webView _obscuredContentInsets], initialInsets));

    auto finalInsets = NSEdgeInsetsMake(50, 100, 0, 10);
    [webView setObscuredContentInsets:finalInsets];
    EXPECT_TRUE(NSEdgeInsetsEqual([webView _obscuredContentInsets], finalInsets));
}

TEST(ObscuredContentInsets, ScrollViewFrameWithObscuredInsets)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 150, 30, 10)];
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    EXPECT_EQ([webView scrollViewFrame], NSMakeRect(150, 0, 640, 600));
}

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)

TEST(ObscuredContentInsets, ResizeScrollPocketWithoutHorizontalBannerView)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 600)]);
#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)
    [webView _disableColorExtensionBehaviorForHorizontalBannerViewOverlaysForTesting];
#endif
    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 100, 0, 0)];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(NSMakeRect(0, 0, 400, 100), [webView _topScrollPocket].frame);

    [webView setFrame:NSMakeRect(0, 0, 800, 600)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(NSMakeRect(0, 0, 800, 100), [webView _topScrollPocket].frame);
}

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)

TEST(ObscuredContentInsets, ResizeScrollPocketWithHorizontalBannerView)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 600)]);
    [webView _enableColorExtensionBehaviorForHorizontalBannerViewOverlaysForTesting];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 100, 0, 0)];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(NSMakeRect(100, 0, 300, 100), [webView _topScrollPocket].frame);

    [webView setFrame:NSMakeRect(0, 0, 800, 600)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(NSMakeRect(100, 0, 700, 100), [webView _topScrollPocket].frame);
}

#endif

TEST(ObscuredContentInsets, ScrollPocketCaptureColor)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [webView setFrame:NSMakeRect(0, 0, 600, 400)];
    [webView waitForNextPresentationUpdate];

    RetainPtr initialColor = [[webView _topScrollPocket] captureColor];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    auto colorBeforeChangingBackground = WebCore::colorFromCocoaColor([[webView _topScrollPocket] captureColor]);

    [webView stringByEvaluatingJavaScript:@"document.body.style.backgroundColor = '#222'"];
    [webView waitForNextPresentationUpdate];

    auto colorAfterChangingBackground = WebCore::colorFromCocoaColor([[webView _topScrollPocket] captureColor]);

    EXPECT_TRUE([initialColor isEqual:NSColor.controlBackgroundColor]);
    EXPECT_EQ(WebCore::serializationForCSS(colorBeforeChangingBackground), "rgb(255, 255, 255)"_s);
    EXPECT_EQ(WebCore::serializationForCSS(colorAfterChangingBackground), "rgb(34, 34, 34)"_s);
}
// FIXME when rdar://164512771 is resolved.
#if PLATFORM(MAC)
TEST(ObscuredContentInsets, DISABLED_TopOverhangColorExtensionLayer)
#else
TEST(ObscuredContentInsets, TopOverhangColorExtensionLayer)
#endif
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);

    [webView setAllowsMagnification:YES];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    __block RetainPtr<CALayer> colorExtensionLayer;
    __block RetainPtr<CALayer> rootContentLayer;
    [webView forEachCALayer:^(CALayer *layer) {
        if ([layer.name containsString:@"top overhang"])
            colorExtensionLayer = layer;
        else if ([layer.name containsString:@"content root"])
            rootContentLayer = layer;

        if (colorExtensionLayer && rootContentLayer)
            return IterationStatus::Done;

        return IterationStatus::Continue;
    }];

    auto sanityCheckColorExtensionLayer = ^{
        auto colorExtensionLayerFrame = [colorExtensionLayer frame];
        auto rootContentLayerFrame = [rootContentLayer frame];
        EXPECT_FALSE(NSIsEmptyRect(colorExtensionLayerFrame));
        EXPECT_TRUE(WTF::areEssentiallyEqual<float>(NSWidth(colorExtensionLayerFrame), NSWidth(rootContentLayerFrame)));
        EXPECT_TRUE(WTF::areEssentiallyEqual<float>(NSMaxY(colorExtensionLayerFrame), NSMinY(rootContentLayerFrame)));

        auto expectedColor = [webView _sampledTopFixedPositionContentColor];
        auto actualColor = [NSColor colorWithCGColor:[colorExtensionLayer backgroundColor]];
        EXPECT_TRUE(Util::compareColors(actualColor, expectedColor));
    };

    sanityCheckColorExtensionLayer();

    [webView setMagnification:2 centeredAtPoint:NSMakePoint(300, 0)];
    [webView waitForNextPresentationUpdate];

    sanityCheckColorExtensionLayer();
}

TEST(ObscuredContentInsets, TopScrollPocketKVO)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);
    RetainPtr observer = adoptNS([[TopScrollPocketObserver alloc] initWithWebView:webView.get()]);

    EXPECT_NULL([webView _topScrollPocket]);
    EXPECT_EQ([observer changeCount], 0u);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    EXPECT_NOT_NULL([webView _topScrollPocket]);
    EXPECT_EQ([observer changeCount], 1u);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];
    [webView waitForNextPresentationUpdate];
    EXPECT_NULL([webView _topScrollPocket]);
    EXPECT_EQ([observer changeCount], 2u);
}

TEST(ObscuredContentInsets, NonObscuredTopContentInset)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);
    [webView _setAutomaticallyAdjustsContentInsets:NO];
    EXPECT_NULL([webView _topScrollPocket]);

    [webView _setTopContentInset:50];
    [webView waitForNextPresentationUpdate];
    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    EXPECT_NULL([webView _topScrollPocket]);

    [webView _setOverrideTopScrollEdgeEffectColor:NSColor.redColor];
    [webView waitForNextPresentationUpdate];
    EXPECT_NOT_NULL([webView _topScrollPocket]);
}

TEST(ObscuredContentInsets, ScrollPocketWithAutomaticTopContentInset)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);
    RetainPtr window = [webView window];

    [window setStyleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView];
    [window setTitlebarAppearsTransparent:NO];
    [window setToolbarStyle:NSWindowToolbarStyleExpanded];
    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView evaluateJavaScript:@"scrollBy(0, 1000)" completionHandler:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_NOT_NULL([webView _topScrollPocket]);
}

TEST(ObscuredContentInsets, PreferSolidColorHardPocket)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);
    EXPECT_NULL([webView _topScrollPocket]);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [webView waitForNextPresentationUpdate];
    [webView synchronouslyLoadTestPageNamed:@"lots-of-text"];

    RetainPtr topScrollPocket = [webView _topScrollPocket];
    EXPECT_NOT_NULL(topScrollPocket);
#if HAVE(LIQUID_GLASS_ADJUSTMENTS)
    EXPECT_FALSE([topScrollPocket prefersSolidColorHardPocket]);
#else
    EXPECT_TRUE([topScrollPocket prefersSolidColorHardPocket]); // Solid color is preferred when the page is not scrolled.
#endif

    [webView objectByEvaluatingJavaScript:@"scrollTo(0, 100)"];
    Util::waitForConditionWithLogging([topScrollPocket] {
        return ![topScrollPocket prefersSolidColorHardPocket];
    }, 3, @"Expected non-solid color hard pocket");

    [webView _setPrefersSolidColorHardScrollPocket:YES];
    EXPECT_TRUE([topScrollPocket prefersSolidColorHardPocket]);

    [webView _setPrefersSolidColorHardScrollPocket:NO];
    EXPECT_FALSE([topScrollPocket prefersSolidColorHardPocket]);
}

TEST(ObscuredContentInsets, AdjustedColorForTopContentInsetColor)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);
    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    RetainPtr actualTopFixedColor = [NSColor colorWithRed:1 green:0.388235 blue:0.278431 alpha:1];

    RetainPtr blueColor = [NSColor systemBlueColor];
    enum class ColorToUse : bool { Proposed, SystemBlue };
    __block auto colorToUse = ColorToUse::Proposed;
    __block BOOL suppressTopColorExtension = NO;
    [delegate setAdjustedColorForTopContentInsetColor:^(WKWebView *, NSColor *proposedColor) {
        [webView _setShouldSuppressTopColorExtensionView:suppressTopColorExtension];
        return colorToUse == ColorToUse::Proposed ? proposedColor : blueColor.get();
    }];

    [webView setUIDelegate:delegate.get()];
    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(Util::compareColors([[webView _topScrollPocket] captureColor], actualTopFixedColor.get()));

    colorToUse = ColorToUse::SystemBlue;
    suppressTopColorExtension = YES;
    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(Util::compareColors([[webView _topScrollPocket] captureColor], blueColor.get()));
}

TEST(ObscuredContentInsets, OverflowHeightForTopScrollEdgeEffect)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);

    [webView setFrame:NSMakeRect(0, 0, 600, 400)];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(50, 0, 0, 0)];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    auto checkScrollPocket = [webView] {
        RetainPtr pocket = [webView _topScrollPocket];
        auto color = WebCore::serializationForCSS(WebCore::colorFromCocoaColor([pocket captureColor]));
        auto rect = [pocket convertRect:[pocket bounds] toView:nil];
        EXPECT_EQ(rect, NSMakeRect(0, 350, 600, 50));
        EXPECT_WK_STREQ(color, "rgb(255, 99, 71)"_s);
    };

    checkScrollPocket();

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];
    [webView setFrame:NSMakeRect(0, 0, 600, 350)];
    [webView _setOverflowHeightForTopScrollEdgeEffect:50];
    [webView waitForNextPresentationUpdate];

    checkScrollPocket();
}

TEST(ObscuredContentInsets, TopOverhangColorExtensionLayerAppearsImmediatelyAfterReload)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    RetainPtr<CALayer> layerBeforeReload;
    Util::waitForConditionWithLogging([&] {
        layerBeforeReload = [webView firstLayerWithNameContaining:@"top overhang"];
        return !!layerBeforeReload;
    }, 3, @"Timed out waiting for top overhang color extension layer to appear");

    RetainPtr expectedColor = [webView _sampledTopFixedPositionContentColor];
    EXPECT_NOT_NULL(expectedColor.get());

    RetainPtr actualColorBeforeReload = [NSColor colorWithCGColor:[layerBeforeReload backgroundColor]];
    EXPECT_TRUE(Util::compareColors(actualColorBeforeReload, expectedColor.get()));

    __block bool layerChecked = false;
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidCommitNavigation:^(WKWebView *view, WKNavigation *) {
        [view _doAfterNextPresentationUpdate:^{
            layerChecked = true;
        }];
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView reload];
    Util::run(&layerChecked);

    RetainPtr layerAfterReload = [webView firstLayerWithNameContaining:@"top overhang"];
    EXPECT_NOT_NULL(layerAfterReload.get());

    if (layerAfterReload) {
        RetainPtr actualColorAfterReload = [NSColor colorWithCGColor:[layerAfterReload backgroundColor]];
        EXPECT_TRUE(Util::compareColors(actualColorAfterReload, expectedColor.get()));
    }
}

TEST(ObscuredContentInsets, TopOverhangColorExtensionLayerRemovedQuicklyAfterNavigatingToPageWithoutFixedHeader)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 0, 0, 0)];
    [webView waitForNextPresentationUpdate];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    RetainPtr<CALayer> layerBeforeReload;
    Util::waitForConditionWithLogging([&] {
        layerBeforeReload = [webView firstLayerWithNameContaining:@"top overhang"];
        return !!layerBeforeReload;
    }, 3, @"Timed out waiting for top overhang color extension layer to appear");

    [webView synchronouslyLoadHTMLString:@"<body style='height:4000px'>No fixed header</body>"];

    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_NULL([webView firstLayerWithNameContaining:@"top overhang"]);
}

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)

namespace HorizontalBannerOverlays {

static constexpr CGFloat viewWidth = 800;
static constexpr CGFloat viewHeight = 600;
static constexpr CGFloat topInset = 60;
static constexpr CGFloat leftInset = 200;
static constexpr CGFloat rightInset = 100;
static constexpr CGFloat overflowPageWidth = 3000;

static RetainPtr<TestWKWebView> setUpWebView(NSString *html)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);
    [webView _setAutomaticallyAdjustsContentInsets:NO];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(topInset, leftInset, 0, rightInset)];
    [webView setAllowsMagnification:YES];
    [webView _enableColorExtensionBehaviorForHorizontalBannerViewOverlaysForTesting];
    [webView synchronouslyLoadHTMLString:html];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];
    return webView;
}

struct PageState {
    double scrollXCSS;
    double minScrollXCSS;
    double scrollWidthCSS;
    double scale;
    BOOL isRTL;
};

static PageState capturePageState(TestWKWebView *webView)
{
    static constexpr NSString *script = @(R"(
        (() => {
            let dir = getComputedStyle(document.documentElement).direction;
            let scrollLeft = document.scrollingElement.scrollLeft;
            let scrollWidth = document.documentElement.scrollWidth;
            let clientWidth = document.scrollingElement.clientWidth;
            let minScrollLeft = dir === 'rtl' ? -(scrollWidth - clientWidth) : 0;
            return JSON.stringify([scrollLeft, minScrollLeft, scrollWidth, dir === 'rtl']);
        })()
        )");

    RetainPtr json = [webView objectByEvaluatingJavaScript:script];
    RetainPtr data = [json dataUsingEncoding:NSUTF8StringEncoding];
    RetainPtr values = [NSJSONSerialization JSONObjectWithData:data.get() options:0 error:nil];
    return PageState {
        [[values objectAtIndex:0] doubleValue],
        [[values objectAtIndex:1] doubleValue],
        [[values objectAtIndex:2] doubleValue],
        [webView magnification],
        [[values objectAtIndex:3] boolValue],
    };
}

static void scrollTo(TestWKWebView *webView, double xCSS)
{
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.scrollTo(%f, 0)", xCSS]];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];
}

struct ExpectedFrames {
    CGRect leftSystemBackground;
    std::optional<CGRect> leftFixed;
    CGRect topScrollPocket;
    CGRect rightSystemBackground;
    std::optional<CGRect> rightFixed;
};

static ExpectedFrames computeExpectedFrames(const PageState& state, bool leftFixedEdge, bool rightFixedEdge)
{
    auto scrollOffsetView = std::trunc((state.scrollXCSS - state.minScrollXCSS) * state.scale);
    auto contentsWidthView = std::trunc(state.scrollWidthCSS * state.scale);

    auto distLeft = leftInset - scrollOffsetView;
    auto distRight = viewWidth - leftInset - contentsWidthView + scrollOffsetView;

    auto clampedLeft = std::clamp<CGFloat>(distLeft, 0, leftInset);
    auto clampedRight = std::clamp<CGFloat>(distRight, 0, rightInset);

    ExpectedFrames frames;
    frames.leftSystemBackground = CGRectMake(std::min<CGFloat>(distLeft - leftInset, 0), 0, leftInset, viewHeight);
    frames.rightSystemBackground = CGRectMake(viewWidth - rightInset + std::max<CGFloat>(rightInset - distRight, 0), 0, rightInset, viewHeight);
    frames.topScrollPocket = CGRectMake(clampedLeft, 0, viewWidth - clampedLeft - clampedRight, topInset);

    if (leftFixedEdge) {
        CGFloat width = std::clamp<CGFloat>(leftInset - distLeft, 0, leftInset);
        frames.leftFixed = CGRectMake(leftInset - width, 0, width, viewHeight);
    }

    if (rightFixedEdge) {
        CGFloat width = std::clamp<CGFloat>(rightInset - distRight, 0, rightInset);
        frames.rightFixed = CGRectMake(viewWidth - rightInset, 0, width, viewHeight);
    }

    return frames;
}

static void expectFrameEqualWithTolerance(CGRect actual, CGRect expected, CGFloat tolerance, NSString *fieldLabel)
{
    bool match = std::fabs(actual.origin.x - expected.origin.x) <= tolerance
        && std::fabs(actual.origin.y - expected.origin.y) <= tolerance
        && std::fabs(actual.size.width - expected.size.width) <= tolerance
        && std::fabs(actual.size.height - expected.size.height) <= tolerance;
    EXPECT_TRUE(match) << fieldLabel.UTF8String << ": actual=" << NSStringFromRect(actual).UTF8String
        << " expected=" << NSStringFromRect(expected).UTF8String
        << " (tolerance=" << tolerance << ")";
}

static void verifyFrames(TestWKWebView *webView, const ExpectedFrames& expected, NSString *label, CGFloat tolerance = 0)
{
    SCOPED_TRACE(label.UTF8String);

    RetainPtr leftSystemBackground = [webView firstLayerWithNameContaining:@"Left system background color extension"];
    EXPECT_NOT_NULL(leftSystemBackground.get());
    if (leftSystemBackground)
        expectFrameEqualWithTolerance([leftSystemBackground frame], expected.leftSystemBackground, tolerance, @"leftSystemBackground");

    RetainPtr leftFixed = [webView firstLayerWithNameContaining:@"Fixed color extension fill (Left)"];
    if (expected.leftFixed) {
        EXPECT_NOT_NULL(leftFixed.get());
        if (leftFixed)
            expectFrameEqualWithTolerance([leftFixed frame], *expected.leftFixed, tolerance, @"leftFixed");
    } else
        EXPECT_NULL(leftFixed.get());

    RetainPtr topScrollPocket = [webView _topScrollPocket];
    EXPECT_NOT_NULL(topScrollPocket.get());
    if (topScrollPocket)
        expectFrameEqualWithTolerance([topScrollPocket frame], expected.topScrollPocket, tolerance, @"topScrollPocket");

    RetainPtr rightSystemBackground = [webView firstLayerWithNameContaining:@"Right system background color extension"];
    EXPECT_NOT_NULL(rightSystemBackground.get());
    if (rightSystemBackground)
        expectFrameEqualWithTolerance([rightSystemBackground frame], expected.rightSystemBackground, tolerance, @"rightSystemBackground");

    RetainPtr rightFixed = [webView firstLayerWithNameContaining:@"Fixed color extension fill (Right)"];
    if (expected.rightFixed) {
        EXPECT_NOT_NULL(rightFixed.get());
        if (rightFixed)
            expectFrameEqualWithTolerance([rightFixed frame], *expected.rightFixed, tolerance, @"rightFixed");
    } else
        EXPECT_NULL(rightFixed.get());
}

static void runSubcases(TestWKWebView *webView, bool leftFixedEdge, bool rightFixedEdge)
{
    auto initialState = capturePageState(webView);
    // For LTR, scrolling toward trailing means increasing scrollLeft; for RTL it means decreasing it.
    auto trailingDirection = initialState.isRTL ? -1.0 : 1.0;
    auto initialScrollLeft = initialState.scrollXCSS;
    auto contentsWidthCSS = initialState.scrollWidthCSS;
    // We need a couple pixels of tolerance to avoid rounding issues.
    auto tolerance = initialState.isRTL ? 2.0 : 0.0;
    auto scrollByOffsetFromLeading = [&](double offsetCSS) {
        scrollTo(webView, initialScrollLeft + trailingDirection * offsetCSS);
    };
    auto verifyAtCurrentState = [&](NSString *label) {
        verifyFrames(webView, computeExpectedFrames(capturePageState(webView), leftFixedEdge, rightFixedEdge), label, tolerance);
    };

    verifyAtCurrentState(@"A: page load");

    scrollByOffsetFromLeading(leftInset / 2);
    verifyAtCurrentState(@"B: scrolled half-leading-inset");

    static constexpr auto zoomScale = 1.5;
    auto zoomCenterX = initialState.isRTL ? (viewWidth - rightInset) : leftInset;
    [webView setMagnification:zoomScale centeredAtPoint:NSMakePoint(zoomCenterX, topInset)];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];
    scrollByOffsetFromLeading((leftInset * 3.0 / 4.0) / zoomScale);
    verifyAtCurrentState(@"C: zoom + scroll for 25% visible");

    scrollByOffsetFromLeading(contentsWidthCSS / 2);
    verifyAtCurrentState(@"D: middle of page");

    scrollByOffsetFromLeading(contentsWidthCSS);
    verifyAtCurrentState(@"E: trailing edge of page");
}

} // namespace HorizontalBannerOverlays

TEST(ObscuredContentInsets, HorizontalBannerOverlaysNoOverflow)
{
    using namespace HorizontalBannerOverlays;

    static constexpr NSString *html = @(R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin: 0; padding: 0 } 
                body { background: red; height: 1000px }
            </style>
        </head>
        <body>
        </body>
        </html>
        )");

    RetainPtr webView = setUpWebView(html);
    runSubcases(webView.get(), false, false);
}

TEST(ObscuredContentInsets, HorizontalBannerOverlaysLeftToRightOverflow)
{
    using namespace HorizontalBannerOverlays;

    RetainPtr html = [@(R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin: 0; padding: 0 }
                body { background: red; height: 1000px; width: __WIDTH__px }
            </style>
        </head>
        <body>
        </body>
        </html>
        )") stringByReplacingOccurrencesOfString:@"__WIDTH__" withString:[NSString stringWithFormat:@"%f", overflowPageWidth]];

    RetainPtr webView = setUpWebView(html.get());
    runSubcases(webView.get(), false, false);
}

TEST(ObscuredContentInsets, HorizontalBannerOverlaysLeftToRightOverflowWithFixedHeader)
{
    using namespace HorizontalBannerOverlays;

    static constexpr NSString *unformattedHTML = @(R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin:0; padding:0 }
                body { background: red; height: 1000px; width: __WIDTH__px }
                header { position: fixed; top: 0; left: 0; width: 100%; height: 60px; background: rgb(255,215,215) }
            </style>
        </head>
        <body>
            <header></header>
        </body>
        </html>
        )");

    RetainPtr html = [unformattedHTML stringByReplacingOccurrencesOfString:@"__WIDTH__" withString:[NSString stringWithFormat:@"%f", overflowPageWidth]];

    RetainPtr webView = setUpWebView(html.get());
    runSubcases(webView.get(), false, false);
}

TEST(ObscuredContentInsets, HorizontalBannerOverlaysLeftToRightOverflowWithFixedHeaderAndSidebar)
{
    using namespace HorizontalBannerOverlays;

    static constexpr NSString *unformattedHTML = @(R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin: 0; padding: 0 }
                body { background: red; height: 1000px; width: __WIDTH__px }
                header { position: fixed; top: 0; left: 0; width: 100%; height: 60px; background: rgb(255,215,215) }
                aside { position: fixed; top: 0; left: 0; width: 200px; height: 100%; background: rgb(215,255,215) }
            </style>
        </head>
        <body>
            <header></header><aside></aside>
        </body>
        </html>
        )");

    RetainPtr html = [unformattedHTML stringByReplacingOccurrencesOfString:@"__WIDTH__" withString:[NSString stringWithFormat:@"%f", overflowPageWidth]];

    RetainPtr webView = setUpWebView(html.get());
    runSubcases(webView.get(), true, false);
}

TEST(ObscuredContentInsets, HorizontalBannerOverlaysRightToLeftOverflow)
{
    using namespace HorizontalBannerOverlays;

    RetainPtr html = [@(R"(
        <!DOCTYPE html>
        <html dir='rtl'>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin: 0; padding: 0 }
                body { background: red; height: 1000px; width: __WIDTH__px }
            </style>
        </head>
        <body>
        </body>
        </html>
        )") stringByReplacingOccurrencesOfString:@"__WIDTH__" withString:[NSString stringWithFormat:@"%f", overflowPageWidth]];

    RetainPtr webView = setUpWebView(html.get());
    runSubcases(webView.get(), false, false);
}

TEST(ObscuredContentInsets, HorizontalBannerOverlaysRightToLeftOverflowWithFixedHeader)
{
    using namespace HorizontalBannerOverlays;

    static constexpr NSString *unformattedHTML = @(R"(
        <!DOCTYPE html>
        <html dir='rtl'>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin:0; padding:0 }
                body { background: red; height: 1000px; width: __WIDTH__px }
                header { position: fixed; top: 0; left: 0; width: 100%; height: 60px; background: rgb(255,215,215) }
            </style>
        </head>
        <body>
            <header></header>
        </body>
        </html>
        )");

    RetainPtr html = [unformattedHTML stringByReplacingOccurrencesOfString:@"__WIDTH__" withString:[NSString stringWithFormat:@"%f", overflowPageWidth]];

    RetainPtr webView = setUpWebView(html.get());
    runSubcases(webView.get(), false, false);
}

TEST(ObscuredContentInsets, HorizontalBannerOverlaysRightToLeftOverflowWithFixedHeaderAndSidebar)
{
    using namespace HorizontalBannerOverlays;

    static constexpr NSString *unformattedHTML = @(R"(
        <!DOCTYPE html>
        <html dir='rtl'>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin: 0; padding: 0 }
                body { background: red; height: 1000px; width: __WIDTH__px }
                header { position: fixed; top: 0; left: 0; width: 100%; height: 60px; background: rgb(255,215,215) }
                aside { position: fixed; top: 0; right: 0; width: 100px; height: 100%; background: rgb(215,255,215) }
            </style>
        </head>
        <body>
            <header></header><aside></aside>
        </body>
        </html>
        )");

    RetainPtr html = [unformattedHTML stringByReplacingOccurrencesOfString:@"__WIDTH__" withString:[NSString stringWithFormat:@"%f", overflowPageWidth]];

    RetainPtr webView = setUpWebView(html.get());
    runSubcases(webView.get(), false, true);
}

TEST(ObscuredContentInsets, HorizontalBannerOverlaysSystemBackgroundColorTracksAppearance)
{
    using namespace HorizontalBannerOverlays;

    RetainPtr html = [@(R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name='viewport' content='width=device-width,initial-scale=1'>
            <style>
                html, body { margin: 0; padding: 0 }
                body { background: red; height: 1000px; width: __WIDTH__px }
            </style>
        </head>
        <body>
        </body>
        </html>
        )") stringByReplacingOccurrencesOfString:@"__WIDTH__" withString:[NSString stringWithFormat:@"%f", overflowPageWidth]];

    auto resolveWindowBackgroundColor = [](NSAppearance *appearance) {
        __block RetainPtr<NSColor> resolved;
        [appearance performAsCurrentDrawingAppearance:^{
            resolved = [NSColor colorWithCGColor:[NSColor windowBackgroundColor].CGColor];
        }];
        return resolved;
    };

    auto leftSystemBackgroundColor = [](TestWKWebView *webView) {
        RetainPtr layer = [webView firstLayerWithNameContaining:@"Left system background color extension"];
        EXPECT_NOT_NULL(layer.get());
        if (!layer)
            return RetainPtr<NSColor> { };
        return RetainPtr { [NSColor colorWithCGColor:[layer backgroundColor]] };
    };

    RetainPtr aquaAppearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    RetainPtr darkAquaAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);
    [webView _setAutomaticallyAdjustsContentInsets:NO];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(topInset, leftInset, 0, rightInset)];
    [webView _enableColorExtensionBehaviorForHorizontalBannerViewOverlaysForTesting];
    [webView setAppearance:aquaAppearance.get()];
    [webView synchronouslyLoadHTMLString:html.get()];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    auto expectedAquaColor = resolveWindowBackgroundColor(aquaAppearance.get());
    EXPECT_TRUE(Util::compareColors(leftSystemBackgroundColor(webView.get()).get(), expectedAquaColor.get()));

    [webView _cancelFixedColorExtensionFadeAnimationsForTesting];
    [webView setAppearance:darkAquaAppearance.get()];
    [webView waitForNextPresentationUpdate];
    [webView _cancelFixedColorExtensionFadeAnimationsForTesting];

    auto expectedDarkColor = resolveWindowBackgroundColor(darkAquaAppearance.get());
    EXPECT_TRUE(Util::compareColors(leftSystemBackgroundColor(webView.get()).get(), expectedDarkColor.get()));
    // Sanity check to ensure this test is actually meaningful.
    EXPECT_FALSE(Util::compareColors(expectedAquaColor.get(), expectedDarkColor.get()));
}

#endif // ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)

#if ENABLE(SCROLL_POCKET_IN_FULLSCREEN)

TEST(ObscuredContentInsets, ScrollPocketCoversFullScreenTitlebar)
{
    constexpr CGFloat topInset = 20;

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setAutomaticallyAdjustsContentInsets:NO];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(topInset, 0, 0, 0)];
    [webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    auto styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    RetainPtr toolbar = adoptNS([[NSToolbar alloc] initWithIdentifier:@"ScrollPocketTestToolbar"]);
    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO]);

    [window setCollectionBehavior:[window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary];
    [window setToolbar:toolbar.get()];
    [[window contentView] addSubview:webView.get()];
    [webView setFrame:[[window contentView] bounds]];
    [window makeKeyAndOrderFront:nil];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    EXPECT_NOT_NULL([webView _topScrollPocket]);
    EXPECT_EQ(0, [webView _fullScreenTitlebarOverlayHeightForTesting]);
    EXPECT_EQ(topInset, NSHeight([[webView _topScrollPocket] frame]));

    __block bool didEnter = false;
    __block bool didExit = false;
    RetainPtr enterObserver = [NSNotificationCenter.defaultCenter addObserverForName:NSWindowDidEnterFullScreenNotification object:window.get() queue:nil usingBlock:^(NSNotification *) {
        didEnter = true;
    }];
    RetainPtr exitObserver = [NSNotificationCenter.defaultCenter addObserverForName:NSWindowDidExitFullScreenNotification object:window.get() queue:nil usingBlock:^(NSNotification *) {
        didExit = true;
    }];

    [window toggleFullScreen:nil];
    EXPECT_TRUE(Util::runFor(&didEnter, 10_s));
    [webView waitForNextPresentationUpdate];

    auto overlayHeight = [webView _fullScreenTitlebarOverlayHeightForTesting];
    EXPECT_GT(overlayHeight, topInset);
    EXPECT_EQ(overlayHeight, NSHeight([[webView _topScrollPocket] frame]));

    RetainPtr expectedColor = [webView _sampledTopFixedPositionContentColor];
    EXPECT_NOT_NULL(expectedColor.get());
    EXPECT_TRUE(Util::compareColors([[webView _topScrollPocket] captureColor], expectedColor.get()));

    [window toggleFullScreen:nil];
    EXPECT_TRUE(Util::runFor(&didExit, 10_s));
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(0, [webView _fullScreenTitlebarOverlayHeightForTesting]);
    EXPECT_EQ(topInset, NSHeight([[webView _topScrollPocket] frame]));

    [NSNotificationCenter.defaultCenter removeObserver:enterObserver.get()];
    [NSNotificationCenter.defaultCenter removeObserver:exitObserver.get()];
}

#endif // ENABLE(SCROLL_POCKET_IN_FULLSCREEN)

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
