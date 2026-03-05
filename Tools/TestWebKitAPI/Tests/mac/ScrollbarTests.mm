/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CGImagePixelReader.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/Color.h>
#import <WebCore/CornerRadii.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if HAVE(NSVIEW_CORNER_CONFIGURATION)

@interface ContainerView : NSView
@property (nonatomic) CGFloat topLeftRadius;
@property (nonatomic) CGFloat topRightRadius;
@property (nonatomic) CGFloat bottomLeftRadius;
@property (nonatomic) CGFloat bottomRightRadius;
@end

@implementation ContainerView

- (void)setCustomCornerRadius:(WebCore::CornerRadii)cornerRadii
{
    _topLeftRadius = cornerRadii.topLeft().width();
    _topRightRadius = cornerRadii.topRight().width();
    _bottomLeftRadius = cornerRadii.bottomLeft().width();
    _bottomRightRadius = cornerRadii.bottomRight().width();
    [self _invalidateCornerConfiguration];
}

- (NSViewCornerConfiguration *)_cornerConfiguration
{
    return [NSViewCornerConfiguration
        configurationWithTopLeftRadius:[_NSCornerRadius fixedRadius:self.topLeftRadius]
        topRightRadius:[_NSCornerRadius fixedRadius:self.topRightRadius]
        bottomLeftRadius:[_NSCornerRadius fixedRadius:self.bottomLeftRadius]
        bottomRightRadius:[_NSCornerRadius fixedRadius:self.bottomRightRadius]];
}

@end

#endif

namespace TestWebKitAPI {

#if HAVE(NSVIEW_CORNER_CONFIGURATION)
enum class ScrollbarType : uint8_t {
    Horizontal,
    Vertical
};
#endif

static double scrollbarLuminanceForWebView(WKWebView *webView)
{
    RetainPtr snapshotImage = adoptNS([webView _windowSnapshotInRect:CGRectNull withOptions:0]);
    RetainPtr snapshotCGImage = [snapshotImage CGImageForProposedRect:NULL context:nil hints:nil];
    CGImagePixelReader reader { snapshotCGImage.get()  };

    auto scrollbarTrackColor = reader.at(reader.width() - 10, reader.height() - 10);
    return scrollbarTrackColor.luminance();
}

TEST(ScrollbarTests, AppearanceChangeAfterSystemAppearanceChange)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
    [webView synchronouslyLoadHTMLString:@"<head><meta name='color-scheme' content='dark light'></head><body style='height: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_GT(scrollbarLuminanceForWebView(webView.get()), 0.5f);

    [webView setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_LT(scrollbarLuminanceForWebView(webView.get()), 0.5f);
}

#if HAVE(NSVIEW_CORNER_CONFIGURATION)

static std::optional<CGRect> scrollbarFrameRect(TestWKWebView *webView, ScrollbarType scrollbarType)
{
    RetainPtr frameRect = scrollbarType == ScrollbarType::Vertical
        ? @"internals.verticalScrollbarFrameRect()"
        : @"internals.horizontalScrollbarFrameRect()";

    RetainPtr script = [NSString stringWithFormat:@"(() => { "
        "const rect = %@; "
        "if (!rect) return null; "
        "return { x: rect.x, y: rect.y, width: rect.width, height: rect.height }; "
        "})()", frameRect.get()];

    id result = [webView objectByEvaluatingJavaScript:script.get()];
    if (RetainPtr<NSDictionary> dictionary = result) {
        return CGRectMake(
            [[dictionary objectForKey:@"x"] doubleValue],
            [[dictionary objectForKey:@"y"] doubleValue],
            [[dictionary objectForKey:@"width"] doubleValue],
            [[dictionary objectForKey:@"height"] doubleValue]
        );
    }

    return std::nullopt;
}

static void verifyVerticalScrollbarFrameRectIsCorrect(RetainPtr<TestWKWebView> webView)
{
    auto topLeftRadius = [webView _effectiveCornerRadii].topLeft;
    auto topRightRadius = [webView _effectiveCornerRadii].topRight;
    auto bottomLeftRadius = [webView _effectiveCornerRadii].bottomLeft;
    auto bottomRightRadius = [webView _effectiveCornerRadii].bottomRight;

    auto topObscuredContentInset = [webView obscuredContentInsets].top;
    auto bottomObscuredContentInset = [webView obscuredContentInsets].bottom;

    [webView synchronouslyLoadHTMLString:@"<body style='height: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto verticalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    EXPECT_TRUE(verticalFrameRect.has_value());
    if (!verticalFrameRect.has_value())
        return;

    auto scrollCapRadius = verticalFrameRect->size.width / 2;
    auto topInset = std::max(std::max(0.0, topRightRadius - scrollCapRadius), topObscuredContentInset);
    auto bottomInset = std::max(std::max(0.0, bottomRightRadius - scrollCapRadius), bottomObscuredContentInset);

    auto frameHeight = [webView frame].size.height;

    auto actualHeight = verticalFrameRect->size.height;
    auto expectedHeight = frameHeight - topInset - bottomInset;
    auto heightWithinHalfPixelOfExpectedValue = expectedHeight - actualHeight <= 0.5 && expectedHeight - actualHeight >= -0.5;

    auto actualYPosition = verticalFrameRect->origin.y;
    auto expectedYPosition = topInset;
    auto yPositionWithinHalfPixelOfExpectedValue = expectedYPosition - actualYPosition <= 0.5 && expectedYPosition - actualYPosition >= -0.5;

    EXPECT_TRUE(yPositionWithinHalfPixelOfExpectedValue);
    EXPECT_TRUE(heightWithinHalfPixelOfExpectedValue);

    [webView synchronouslyLoadHTMLString:@"<html dir='rtl'><body style='height: 2000px;'></body></html>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    verticalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    EXPECT_TRUE(verticalFrameRect.has_value());
    if (!verticalFrameRect.has_value())
        return;

    topInset = std::max(std::max(0.0, topLeftRadius - scrollCapRadius), topObscuredContentInset);
    bottomInset = std::max(std::max(0.0, bottomLeftRadius - scrollCapRadius), bottomObscuredContentInset);

    actualHeight = verticalFrameRect->size.height;
    expectedHeight = frameHeight - topInset - bottomInset;
    heightWithinHalfPixelOfExpectedValue = expectedHeight - actualHeight <= 0.5 && expectedHeight - actualHeight >= -0.5;

    actualYPosition = verticalFrameRect->origin.y;
    expectedYPosition = topInset;
    yPositionWithinHalfPixelOfExpectedValue = expectedYPosition - actualYPosition <= 0.5 && expectedYPosition - actualYPosition >= -0.5;

    EXPECT_TRUE(yPositionWithinHalfPixelOfExpectedValue);
    EXPECT_TRUE(heightWithinHalfPixelOfExpectedValue);
}

static void verifyHorizontalScrollbarFrameRectIsCorrect(RetainPtr<TestWKWebView> webView)
{
    auto bottomLeftRadius = [webView _effectiveCornerRadii].bottomLeft;
    auto bottomRightRadius = [webView _effectiveCornerRadii].bottomRight;

    auto leftObscuredContentInset = webView.get().obscuredContentInsets.left;
    auto rightObscuredContentInset = webView.get().obscuredContentInsets.right;

    [webView synchronouslyLoadHTMLString:@"<body style='width: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto horizontalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    EXPECT_TRUE(horizontalFrameRect.has_value());
    if (!horizontalFrameRect.has_value())
        return;

    auto scrollCapRadius = horizontalFrameRect->size.height / 2;
    auto leftInset = std::max(std::max(0.0, bottomLeftRadius - scrollCapRadius), leftObscuredContentInset);
    auto rightInset = std::max(std::max(0.0, bottomRightRadius - scrollCapRadius), rightObscuredContentInset);

    auto frameWidth = [webView frame].size.width;
    auto actualWidth = horizontalFrameRect->size.width;
    auto expectedWidth = frameWidth - leftInset - rightInset;

    auto actualXPosition = horizontalFrameRect->origin.x;
    auto expectedXPosition = leftInset;
    auto xPositionWithinHalfPixelOfExpectedValue = expectedXPosition - actualXPosition <= 0.5 && expectedXPosition - actualXPosition >= -0.5;

    auto widthWithinHalfPixelOfExpectedValue = expectedWidth - actualWidth <= 0.5 && expectedWidth - actualWidth >= -0.5;

    EXPECT_TRUE(xPositionWithinHalfPixelOfExpectedValue);
    EXPECT_TRUE(widthWithinHalfPixelOfExpectedValue);
}

static void verifyScrollbarFrameRectsAreCorrect(RetainPtr<TestWKWebView> webView)
{
    verifyVerticalScrollbarFrameRectIsCorrect(webView);
    verifyHorizontalScrollbarFrameRectIsCorrect(webView);
}

static void verifyWebViewHasExpectedCornerRadii(RetainPtr<TestWKWebView> webView, WebCore::CornerRadii cornerRadii)
{
    auto topLeftRadius = [webView _effectiveCornerRadii].topLeft;
    auto topRightRadius = [webView _effectiveCornerRadii].topRight;
    auto bottomLeftRadius = [webView _effectiveCornerRadii].bottomLeft;
    auto bottomRightRadius = [webView _effectiveCornerRadii].bottomRight;

    EXPECT_EQ(topLeftRadius, cornerRadii.topLeft().width());
    EXPECT_EQ(topRightRadius, cornerRadii.topRight().width());
    EXPECT_EQ(bottomLeftRadius, cornerRadii.bottomLeft().width());
    EXPECT_EQ(bottomRightRadius, cornerRadii.bottomRight().width());
}

static RetainPtr<TestWKWebView> scrollbarAvoidanceTestWebView()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 500, 400) configuration:configuration.get()]);
}

static RetainPtr<NSWindow> scrollbarAvoidanceTestWindow()
{
    return adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 500, 400)
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
        backing:NSBackingStoreBuffered
        defer:NO]);
}

TEST(ScrollbarTests, ScrollbarAvoidanceForTitledWindow)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();
    [[window contentView] addSubview:webView.get()];

    EXPECT_TRUE([webView _effectiveCornerRadii].bottomRight);
    verifyScrollbarFrameRectsAreCorrect(webView);
}

TEST(ScrollbarTests, ScrollbarAvoidanceForWindowWithUnifiedCompactToolbar)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();
    RetainPtr toolbar = adoptNS([[NSToolbar alloc] initWithIdentifier:@"TestToolbar"]);

    [window setToolbar:toolbar.get()];
    [window setToolbarStyle:NSWindowToolbarStyleUnifiedCompact];
    [[window contentView] addSubview:webView.get()];

    EXPECT_TRUE([webView _effectiveCornerRadii].bottomRight);
    verifyScrollbarFrameRectsAreCorrect(webView);
}

TEST(ScrollbarTests, ScrollbarAvoidanceForWindowWithUnifiedToolbar)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();

    RetainPtr toolbar = adoptNS([[NSToolbar alloc] initWithIdentifier:@"TestToolbar"]);
    [window setToolbar:toolbar.get()];
    [window setToolbarStyle:NSWindowToolbarStyleUnified];
    [[window contentView] addSubview:webView.get()];

    EXPECT_TRUE([webView _effectiveCornerRadii].bottomRight);
    verifyScrollbarFrameRectsAreCorrect(webView);
}

TEST(ScrollbarTests, ScrollbarAvoidanceNoCornerRadii)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();

    [webView synchronouslyLoadHTMLString:@"<body style='width: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto horizontalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    EXPECT_TRUE(horizontalFrameRect.has_value());
    if (!horizontalFrameRect.has_value())
        return;

    auto expectedWidth = [webView frame].size.width;
    auto actualWidth = horizontalFrameRect->size.width;

    auto actualXPosition = horizontalFrameRect->origin.x;
    auto expectedXPosition = 0;

    EXPECT_EQ(actualWidth, expectedWidth);
    EXPECT_EQ(actualXPosition, expectedXPosition);

    [webView synchronouslyLoadHTMLString:@"<body style='height: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto verticalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    EXPECT_TRUE(verticalFrameRect.has_value());
    if (!verticalFrameRect.has_value())
        return;

    auto actualHeight = verticalFrameRect->size.height;
    auto expectedHeight = [webView frame].size.height;

    auto actualYPosition = verticalFrameRect->origin.y;
    auto expectedYPosition = 0;

    EXPECT_EQ(actualHeight, expectedHeight);
    EXPECT_EQ(actualYPosition, expectedYPosition);
}

static void runTestCaseWithCornerRadii(RetainPtr<ContainerView> container, RetainPtr<TestWKWebView> webView, WebCore::CornerRadii cornerRadii)
{
    verifyWebViewHasExpectedCornerRadii(webView, cornerRadii);
    verifyScrollbarFrameRectsAreCorrect(webView);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(5, 5, 5, 5)];
    verifyScrollbarFrameRectsAreCorrect(webView);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(5, 5, 0, 0)];
    verifyScrollbarFrameRectsAreCorrect(webView);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(75, 75, 75, 75)];
    verifyScrollbarFrameRectsAreCorrect(webView);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(5, 5, 0, 0)];
    verifyScrollbarFrameRectsAreCorrect(webView);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];
}

TEST(ScrollbarTests, ScrollbarAvoidanceInConcentricContainerWithUniformCornerRadii)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();
    RetainPtr container = adoptNS([[ContainerView alloc] initWithFrame:NSMakeRect(0, 0, 500, 400)]);

    [[window contentView] addSubview:container.get()];

    WebCore::CornerRadii caseOne { 50 };
    WebCore::CornerRadii caseTwo { 10 };
    WebCore::CornerRadii caseThree { 0 };

    [container setCustomCornerRadius:caseOne];
    [container addSubview:webView.get()];

    runTestCaseWithCornerRadii(container, webView, caseOne);

    [container setCustomCornerRadius:caseTwo];
    [webView waitForNextPresentationUpdate];
    runTestCaseWithCornerRadii(container, webView, caseTwo);

    [container setCustomCornerRadius:caseThree];
    [webView waitForNextPresentationUpdate];
    runTestCaseWithCornerRadii(container, webView, caseThree);
}

TEST(ScrollbarTests, ScrollbarAvoidanceInConcentricContainerWithNonUniformCornerRadii)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();
    RetainPtr container = adoptNS([[ContainerView alloc] initWithFrame:NSMakeRect(0, 0, 500, 400)]);

    [[window contentView] addSubview:container.get()];

    WebCore::CornerRadii caseOne { 50, 0, 16, 4 };
    WebCore::CornerRadii caseTwo { 20, 8, 2, 10 };

    [container setCustomCornerRadius:caseOne];
    [container addSubview:webView.get()];

    runTestCaseWithCornerRadii(container, webView, caseOne);

    [container setCustomCornerRadius:caseTwo];
    [webView waitForNextPresentationUpdate];
    runTestCaseWithCornerRadii(container, webView, caseTwo);
}

#endif // HAVE(NSVIEW_CORNER_CONFIGURATION)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
