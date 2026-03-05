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

#import "AppKitSPI.h"
#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestCocoaImageAndCocoaColor.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
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

TEST(ObscuredContentInsets, ResizeScrollPocket)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 600)]);
    [webView setObscuredContentInsets:NSEdgeInsetsMake(100, 100, 0, 0)];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(NSMakeRect(0, 0, 400, 100), [webView _topScrollPocket].frame);

    [webView setFrame:NSMakeRect(0, 0, 800, 600)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(NSMakeRect(0, 0, 800, 100), [webView _topScrollPocket].frame);
}

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
    EXPECT_TRUE([topScrollPocket prefersSolidColorHardPocket]); // Solid color is preferred when the page is not scrolled.

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

#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
