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

#import "ClassMethodSwizzler.h"
#import "Helpers/mac/AppKitSPI.h"
#import "Helpers/cocoa/CGImagePixelReader.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebCore/Color.h>
#import <WebCore/CornerRadii.h>
#import <WebKit/WKSnapshotConfigurationPrivate.h>
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

- (void)setCustomCornerRadius:(::WebCore::CornerRadii)cornerRadii
{
    _topLeftRadius = cornerRadii.topLeft().width();
    _topRightRadius = cornerRadii.topRight().width();
    _bottomLeftRadius = cornerRadii.bottomLeft().width();
    _bottomRightRadius = cornerRadii.bottomRight().width();
    [self _invalidateCornerConfiguration];
}

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
- (NSViewCornerConfiguration *)_cornerConfiguration
{
    return [NSViewCornerConfiguration
        configurationWithTopLeftRadius:(id)[_NSCornerRadius fixedRadius:self.topLeftRadius]
        topRightRadius:(id)[_NSCornerRadius fixedRadius:self.topRightRadius]
        bottomLeftRadius:(id)[_NSCornerRadius fixedRadius:self.bottomLeftRadius]
        bottomRightRadius:(id)[_NSCornerRadius fixedRadius:self.bottomRightRadius]];
}
ALLOW_NEW_API_WITHOUT_GUARDS_END

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

enum class FrameScrollbarsSnapshot : bool { ViewRect, ContentsRect };

// The page and the iframe have vertical scroll bars, and both are green where they do not have scroll bars.
static RetainPtr<TestWKWebView> frameScrollbarsWebView(bool usesOverlayScrollbars)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@""];
    [webView stringByEvaluatingJavaScript:usesOverlayScrollbars ? @"internals.setUsesOverlayScrollbars(true)" : @"internals.setUsesOverlayScrollbars(false)"];
    [webView synchronouslyLoadHTMLString:@"<body style='margin: 0; height: 2000px; background-color: rgb(0, 255, 0)'>"
        "<iframe style='display: block; width: 200px; height: 200px; border: none' srcdoc=\"<body style='margin: 0; height: 2000px; background-color: rgb(0, 255, 0)'></body>\"></iframe></body>"];
    [webView waitForNextPresentationUpdate];
    return webView;
}

static int clientWidthOfPage(TestWKWebView *webView)
{
    return [webView stringByEvaluatingJavaScript:@"document.documentElement.clientWidth"].intValue;
}

static int clientWidthOfIFrame(TestWKWebView *webView)
{
    return [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.documentElement.clientWidth"].intValue;
}

static RetainPtr<CGImageRef> snapshotOfFrameScrollbars(TestWKWebView *webView, FrameScrollbarsSnapshot snapshot)
{
    RetainPtr configuration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [configuration _setUsesContentsRect:snapshot == FrameScrollbarsSnapshot::ContentsRect];

    __block RetainPtr<CGImageRef> image;
    __block bool done = false;
    [webView takeSnapshotWithConfiguration:configuration.get() completionHandler:^(NSImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        image = Util::convertToCGImage(snapshotImage);
        done = true;
    }];
    Util::run(&done);
    return image;
}

static void testSnapshotIncludesFrameScrollbars(FrameScrollbarsSnapshot snapshot)
{
    RetainPtr webView = frameScrollbarsWebView(false);

    // Legacy scroll bars take up space. The scroll bar tracks start where the client area ends.
    int pageClientWidth = clientWidthOfPage(webView.get());
    int iframeClientWidth = clientWidthOfIFrame(webView.get());
    ASSERT_GT(pageClientWidth, 0);
    ASSERT_GT(iframeClientWidth, 0);
    EXPECT_LT(pageClientWidth, 400);
    EXPECT_LT(iframeClientWidth, 200);

    RetainPtr image = snapshotOfFrameScrollbars(webView.get(), snapshot);
    ASSERT_NOT_NULL(image.get());
    CGImagePixelReader reader { image.get() };
    unsigned scale = reader.width() / (snapshot == FrameScrollbarsSnapshot::ContentsRect ? pageClientWidth : 400);
    EXPECT_GE(scale, 1u);

    EXPECT_WK_STREQ("rgb(0, 255, 0)", reader.cssColorAt(50 * scale, 100 * scale));
    EXPECT_WK_STRNE("rgb(0, 255, 0)", reader.cssColorAt((iframeClientWidth + 200) / 2 * scale, 100 * scale));
    EXPECT_WK_STREQ("rgb(0, 255, 0)", reader.cssColorAt(300 * scale, 300 * scale));

    // The snapshot of the contents of the page has no room for the page's scroll bar.
    if (snapshot == FrameScrollbarsSnapshot::ViewRect)
        EXPECT_WK_STRNE("rgb(0, 255, 0)", reader.cssColorAt((pageClientWidth + 400) / 2 * scale, 300 * scale));
}

TEST(ScrollbarTests, SnapshotIncludesFrameScrollbars)
{
    testSnapshotIncludesFrameScrollbars(FrameScrollbarsSnapshot::ViewRect);
}

TEST(ScrollbarTests, SnapshotOfContentsIncludesIFrameScrollbars)
{
    testSnapshotIncludesFrameScrollbars(FrameScrollbarsSnapshot::ContentsRect);
}

// Overlay scroll bars are hidden until the frame scrolls, so a snapshot does not show them. This test
// checks that snapshots keep leaving them out.
TEST(ScrollbarTests, SnapshotDoesNotIncludeHiddenOverlayFrameScrollbars)
{
    RetainPtr webView = frameScrollbarsWebView(true);
    EXPECT_EQ(clientWidthOfPage(webView.get()), 400);
    EXPECT_EQ(clientWidthOfIFrame(webView.get()), 200);

    RetainPtr image = snapshotOfFrameScrollbars(webView.get(), FrameScrollbarsSnapshot::ViewRect);
    ASSERT_NOT_NULL(image.get());
    CGImagePixelReader reader { image.get() };
    unsigned scale = reader.width() / 400;
    EXPECT_GE(scale, 1u);

    for (unsigned x = 185; x < 200; ++x)
        EXPECT_WK_STREQ("rgb(0, 255, 0)", reader.cssColorAt(x * scale, 10 * scale));
    for (unsigned x = 385; x < 400; ++x)
        EXPECT_WK_STREQ("rgb(0, 255, 0)", reader.cssColorAt(x * scale, 10 * scale));
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
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto topLeftRadius = [webView _effectiveCornerRadii].topLeft;
    auto topRightRadius = [webView _effectiveCornerRadii].topRight;
    auto bottomLeftRadius = [webView _effectiveCornerRadii].bottomLeft;
    auto bottomRightRadius = [webView _effectiveCornerRadii].bottomRight;
ALLOW_NEW_API_WITHOUT_GUARDS_END

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
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto bottomLeftRadius = [webView _effectiveCornerRadii].bottomLeft;
    auto bottomRightRadius = [webView _effectiveCornerRadii].bottomRight;
ALLOW_NEW_API_WITHOUT_GUARDS_END

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

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
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
ALLOW_NEW_API_WITHOUT_GUARDS_END

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

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    EXPECT_TRUE([webView _effectiveCornerRadii].bottomRight);
ALLOW_NEW_API_WITHOUT_GUARDS_END
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

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    EXPECT_TRUE([webView _effectiveCornerRadii].bottomRight);
ALLOW_NEW_API_WITHOUT_GUARDS_END
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

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    EXPECT_TRUE([webView _effectiveCornerRadii].bottomRight);
ALLOW_NEW_API_WITHOUT_GUARDS_END
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

TEST(ScrollbarTests, VerticalScrollbarXPositionAccountsForObscuredContentInsets)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();

    [webView synchronouslyLoadHTMLString:@"<body style='height: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto frameWidth = [webView frame].size.width;

    auto verticalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalFrameRect);

    auto scrollbarWidth = verticalFrameRect->size.width;
    EXPECT_EQ(verticalFrameRect->origin.x, frameWidth - scrollbarWidth);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 50)];
    [webView waitForNextPresentationUpdate];

    verticalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalFrameRect);

    EXPECT_EQ(verticalFrameRect->origin.x, frameWidth - scrollbarWidth - 50);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];

    [webView synchronouslyLoadHTMLString:@"<html dir='rtl'><body style='height: 2000px;'></body></html>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    verticalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalFrameRect);

    EXPECT_EQ(verticalFrameRect->origin.x, 0);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 50, 0, 0)];
    [webView waitForNextPresentationUpdate];

    verticalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalFrameRect);

    EXPECT_EQ(verticalFrameRect->origin.x, 50);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];
}

TEST(ScrollbarTests, HorizontalScrollbarYPositionAccountsForObscuredContentInsets)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();

    [webView synchronouslyLoadHTMLString:@"<body style='width: 2000px;'></body>"];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto frameHeight = [webView frame].size.height;

    auto horizontalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    EXPECT_TRUE(horizontalFrameRect.has_value());
    if (!horizontalFrameRect.has_value())
        return;

    auto scrollbarHeight = horizontalFrameRect->size.height;
    EXPECT_EQ(horizontalFrameRect->origin.y, frameHeight - scrollbarHeight);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 50, 0)];
    [webView waitForNextPresentationUpdate];

    horizontalFrameRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    EXPECT_TRUE(horizontalFrameRect.has_value());
    if (!horizontalFrameRect.has_value())
        return;

    EXPECT_EQ(horizontalFrameRect->origin.y, frameHeight - scrollbarHeight - 50);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];
}

static std::optional<CGRect> scrollCornerRectForWebView(TestWKWebView *webView)
{
    RetainPtr script = @"(() => { "
        "const rect = internals.scrollCornerRect(); "
        "if (!rect) return null; "
        "return { x: rect.x, y: rect.y, width: rect.width, height: rect.height }; "
        "})()";

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

static constexpr NSString *customScrollbarStyle = @(R"(
    ::-webkit-scrollbar { width: 10px; }
    ::-webkit-scrollbar:horizontal { height: 18px; }
    ::-webkit-scrollbar-track { background: gray; }
    ::-webkit-scrollbar-thumb { background: yellow; }
    ::-webkit-scrollbar-corner { background: red; }
    )");

static NSString *customScrollbarHTMLWithDimensions(NSString *bodyStyle)
{
    return [NSString stringWithFormat:@"<style>%@</style><body style='%@'></body>", customScrollbarStyle, bodyStyle];
}

static NSString *customScrollbarHTMLWithBothScrollbars()
{
    return customScrollbarHTMLWithDimensions(@"width: 2000px; height: 2000px;");
}

TEST(ScrollbarTests, CustomScrollbarSizesNoCornerRadii)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();

    [webView synchronouslyLoadHTMLString:customScrollbarHTMLWithBothScrollbars()];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto frameWidth = [webView frame].size.width;
    auto frameHeight = [webView frame].size.height;

    auto verticalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalRect);
    EXPECT_EQ(verticalRect->size.width, 10);
    EXPECT_EQ(verticalRect->origin.x, frameWidth - 10);
    EXPECT_EQ(verticalRect->origin.y, 0);
    EXPECT_EQ(verticalRect->size.height, frameHeight - 18);

    auto horizontalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    ASSERT_TRUE(horizontalRect);
    EXPECT_EQ(horizontalRect->size.height, 18);
    EXPECT_EQ(horizontalRect->origin.x, 0);
    EXPECT_EQ(horizontalRect->origin.y, frameHeight - 18);
    EXPECT_EQ(horizontalRect->size.width, frameWidth - 10);
}

TEST(ScrollbarTests, CustomScrollbarScrollCornerRectNoCornerRadii)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();

    [webView synchronouslyLoadHTMLString:customScrollbarHTMLWithBothScrollbars()];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto frameWidth = [webView frame].size.width;
    auto frameHeight = [webView frame].size.height;

    auto cornerRect = scrollCornerRectForWebView(webView.get());
    ASSERT_TRUE(cornerRect);
    EXPECT_EQ(cornerRect->origin.x, frameWidth - 10);
    EXPECT_EQ(cornerRect->origin.y, frameHeight - 18);
    EXPECT_EQ(cornerRect->size.width, 10);
    EXPECT_EQ(cornerRect->size.height, 18);
}

TEST(ScrollbarTests, CustomScrollbarScrollCornerRectWithObscuredContentInsets)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();

    [webView synchronouslyLoadHTMLString:customScrollbarHTMLWithBothScrollbars()];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 15, 20)];
    [webView waitForNextPresentationUpdate];

    auto frameWidth = [webView frame].size.width;
    auto frameHeight = [webView frame].size.height;

    auto verticalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalRect);
    EXPECT_EQ(verticalRect->origin.x, frameWidth - 10 - 20);

    auto horizontalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    ASSERT_TRUE(horizontalRect);
    EXPECT_EQ(horizontalRect->origin.y, frameHeight - 18 - 15);

    auto cornerRect = scrollCornerRectForWebView(webView.get());
    ASSERT_TRUE(cornerRect);
    EXPECT_EQ(cornerRect->origin.x, frameWidth - 10 - 20);
    EXPECT_EQ(cornerRect->origin.y, frameHeight - 18 - 15);
    EXPECT_EQ(cornerRect->size.width, 10);
    EXPECT_EQ(cornerRect->size.height, 18);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];
}

TEST(ScrollbarTests, CustomScrollbarCornerRadiusAvoidanceAccountsForScrollCorner)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();
    RetainPtr container = adoptNS([[ContainerView alloc] initWithFrame:NSMakeRect(0, 0, 500, 400)]);

    [[window contentView] addSubview:container.get()];

    WebCore::CornerRadii radii { 30 };
    [container setCustomCornerRadius:radii];
    [container addSubview:webView.get()];

    [webView synchronouslyLoadHTMLString:customScrollbarHTMLWithBothScrollbars()];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto frameWidth = [webView frame].size.width;
    auto frameHeight = [webView frame].size.height;

    auto verticalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalRect);
    auto horizontalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    ASSERT_TRUE(horizontalRect);

    auto cornerRadius = 30.0;
    auto verticalScrollbarWidth = verticalRect->size.width;
    auto horizontalScrollbarHeight = horizontalRect->size.height;

    auto expectedBottomOffset = std::max(0.0, cornerRadius - horizontalScrollbarHeight);
    auto expectedTopOffset = cornerRadius;
    auto expectedVerticalHeight = frameHeight - horizontalScrollbarHeight - expectedTopOffset - expectedBottomOffset;

    EXPECT_NEAR(verticalRect->origin.y, expectedTopOffset, 0.5);
    EXPECT_NEAR(verticalRect->size.height, expectedVerticalHeight, 0.5);

    auto expectedRightOffset = std::max(0.0, cornerRadius - verticalScrollbarWidth);
    auto expectedLeftOffset = cornerRadius;
    auto expectedHorizontalWidth = frameWidth - verticalScrollbarWidth - expectedLeftOffset - expectedRightOffset;

    EXPECT_NEAR(horizontalRect->origin.x, expectedLeftOffset, 0.5);
    EXPECT_NEAR(horizontalRect->size.width, expectedHorizontalWidth, 0.5);

    auto cornerRect = scrollCornerRectForWebView(webView.get());
    ASSERT_TRUE(cornerRect);
    EXPECT_NEAR(cornerRect->origin.x, verticalRect->origin.x, 0.5);
    EXPECT_NEAR(cornerRect->origin.y, horizontalRect->origin.y, 0.5);
    EXPECT_NEAR(cornerRect->size.width, verticalScrollbarWidth, 0.5);
    EXPECT_NEAR(cornerRect->size.height, horizontalScrollbarHeight, 0.5);
}

TEST(ScrollbarTests, CustomScrollbarCornerRadiusAvoidanceWithObscuredContentInsetsAndScrollCorner)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();
    RetainPtr container = adoptNS([[ContainerView alloc] initWithFrame:NSMakeRect(0, 0, 500, 400)]);

    [[window contentView] addSubview:container.get()];

    WebCore::CornerRadii radii { 30 };
    [container setCustomCornerRadius:radii];
    [container addSubview:webView.get()];

    [webView synchronouslyLoadHTMLString:customScrollbarHTMLWithBothScrollbars()];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView setObscuredContentInsets:NSEdgeInsetsMake(5, 5, 10, 8)];
    [webView waitForNextPresentationUpdate];

    auto frameWidth = [webView frame].size.width;
    auto frameHeight = [webView frame].size.height;

    auto verticalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalRect);
    auto horizontalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    ASSERT_TRUE(horizontalRect);

    auto cornerRadius = 30.0;
    auto verticalScrollbarWidth = 10.0;
    auto horizontalScrollbarHeight = 18.0;

    auto expectedTopOffset = std::max(cornerRadius, 5.0);
    auto expectedBottomOffset = std::max(std::max(0.0, cornerRadius - horizontalScrollbarHeight), 10.0);
    auto expectedVerticalHeight = frameHeight - horizontalScrollbarHeight - expectedTopOffset - expectedBottomOffset;

    EXPECT_NEAR(verticalRect->origin.y, expectedTopOffset, 0.5);
    EXPECT_NEAR(verticalRect->size.height, expectedVerticalHeight, 0.5);
    EXPECT_NEAR(verticalRect->origin.x, frameWidth - verticalScrollbarWidth - 8, 0.5);

    auto expectedLeftOffset = std::max(cornerRadius, 5.0);
    auto expectedRightOffset = std::max(std::max(0.0, cornerRadius - verticalScrollbarWidth), 8.0);
    auto expectedHorizontalWidth = frameWidth - verticalScrollbarWidth - expectedLeftOffset - expectedRightOffset;

    EXPECT_NEAR(horizontalRect->origin.x, expectedLeftOffset, 0.5);
    EXPECT_NEAR(horizontalRect->size.width, expectedHorizontalWidth, 0.5);
    EXPECT_NEAR(horizontalRect->origin.y, frameHeight - horizontalScrollbarHeight - 10, 0.5);

    auto cornerRect = scrollCornerRectForWebView(webView.get());
    ASSERT_TRUE(cornerRect);
    EXPECT_NEAR(cornerRect->origin.x, verticalRect->origin.x, 0.5);
    EXPECT_NEAR(cornerRect->origin.y, horizontalRect->origin.y, 0.5);
    EXPECT_NEAR(cornerRect->size.width, verticalScrollbarWidth, 0.5);
    EXPECT_NEAR(cornerRect->size.height, horizontalScrollbarHeight, 0.5);

    [webView setObscuredContentInsets:NSEdgeInsetsMake(0, 0, 0, 0)];
}

TEST(ScrollbarTests, CustomScrollbarScrollCornerSmallCornerRadius)
{
    RetainPtr webView = scrollbarAvoidanceTestWebView();
    RetainPtr window = scrollbarAvoidanceTestWindow();
    RetainPtr container = adoptNS([[ContainerView alloc] initWithFrame:NSMakeRect(0, 0, 500, 400)]);

    [[window contentView] addSubview:container.get()];

    WebCore::CornerRadii radii { 8 };
    [container setCustomCornerRadius:radii];
    [container addSubview:webView.get()];

    [webView synchronouslyLoadHTMLString:customScrollbarHTMLWithBothScrollbars()];
    [webView stringByEvaluatingJavaScript:@"internals.setUsesOverlayScrollbars(false)"];
    [webView waitForNextPresentationUpdate];

    auto frameWidth = [webView frame].size.width;
    auto frameHeight = [webView frame].size.height;

    auto verticalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Vertical);
    ASSERT_TRUE(verticalRect);
    auto horizontalRect = scrollbarFrameRect(webView.get(), ScrollbarType::Horizontal);
    ASSERT_TRUE(horizontalRect);

    auto cornerRadius = 8.0;
    auto verticalScrollbarWidth = 10.0;
    auto horizontalScrollbarHeight = 18.0;

    auto expectedTopOffset = cornerRadius;
    auto expectedBottomOffset = 0.0;
    auto expectedVerticalHeight = frameHeight - horizontalScrollbarHeight - expectedTopOffset - expectedBottomOffset;

    EXPECT_NEAR(verticalRect->origin.y, expectedTopOffset, 0.5);
    EXPECT_NEAR(verticalRect->size.height, expectedVerticalHeight, 0.5);

    auto expectedLeftOffset = cornerRadius;
    auto expectedRightOffset = 0.0;
    auto expectedHorizontalWidth = frameWidth - verticalScrollbarWidth - expectedLeftOffset - expectedRightOffset;

    EXPECT_NEAR(horizontalRect->origin.x, expectedLeftOffset, 0.5);
    EXPECT_NEAR(horizontalRect->size.width, expectedHorizontalWidth, 0.5);

    auto cornerRect = scrollCornerRectForWebView(webView.get());
    ASSERT_TRUE(cornerRect);
    EXPECT_NEAR(cornerRect->origin.x, verticalRect->origin.x, 0.5);
    EXPECT_NEAR(cornerRect->origin.y, horizontalRect->origin.y, 0.5);
    EXPECT_NEAR(cornerRect->size.width, verticalScrollbarWidth, 0.5);
    EXPECT_NEAR(cornerRect->size.height, horizontalScrollbarHeight, 0.5);
}

#endif // HAVE(NSVIEW_CORNER_CONFIGURATION)

static NSScrollerStyle scrollbarTestsScrollerStyle;

static NSScrollerStyle scrollbarTestsPreferredScrollerStyle(id, SEL)
{
    return scrollbarTestsScrollerStyle;
}

// The right edge, the top and the height of the vertical scroll bar of the node, which don't depend on the scroll bar style.
static NSString *verticalScrollbarGeometry(TestWKWebView *webView, NSString *node)
{
    return [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"(() => { const r = internals.verticalScrollbarFrameRect(%@); return [r.x + r.width, r.y, r.height].join(','); })()", node]];
}

// Its own suite, so that it runs in its own process: it changes the scroller style of the process.
TEST(ScrollerStyleChange, UpdatesIFrameAndOverflowScrollbars)
{
    {
        // Web processes take their scroller style from the UI process.
        scrollbarTestsScrollerStyle = NSScrollerStyleLegacy;
        ClassMethodSwizzler swizzler(NSScroller.class, @selector(preferredScrollerStyle), reinterpret_cast<IMP>(scrollbarTestsPreferredScrollerStyle));

        RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
        [webView synchronouslyLoadHTMLString:@"<body style='margin: 0'>"
            "<iframe style='display: block; width: 200px; height: 200px; border: none' srcdoc=\"<body style='margin: 0; height: 2000px'></body>\"></iframe>"
            "<div id='scroller' style='width: 200px; height: 200px; overflow-y: scroll'><div style='height: 2000px'></div></div></body>"];
        [webView waitForNextPresentationUpdate];

        static NSString *iframeDocument = @"document.querySelector('iframe').contentDocument";
        static NSString *scroller = @"document.getElementById('scroller')";
        NSString *iframeClientWidthScript = [iframeDocument stringByAppendingString:@".documentElement.clientWidth"];
        NSString *scrollerClientWidthScript = [scroller stringByAppendingString:@".clientWidth"];

        auto expectScrollbars = [&](NSString *description) {
            SCOPED_TRACE(description.UTF8String);
            // The iframe's scroll bar spans its right edge, and the scroller's spans the scroller's right edge below the iframe.
            EXPECT_WK_STREQ("200,0,200", verticalScrollbarGeometry(webView.get(), iframeDocument));
            EXPECT_WK_STREQ("200,200,200", verticalScrollbarGeometry(webView.get(), scroller));
        };

        auto changeScrollerStyle = [&](NSScrollerStyle style) {
            scrollbarTestsScrollerStyle = style;
            [NSNotificationCenter.defaultCenter postNotificationName:NSPreferredScrollerStyleDidChangeNotification object:nil];
            // Legacy scroll bars take up space, and overlay scroll bars don't.
            EXPECT_TRUE(Util::waitFor([&] {
                bool isOverlay = style == NSScrollerStyleOverlay;
                int iframeClientWidth = [webView stringByEvaluatingJavaScript:iframeClientWidthScript].intValue;
                int scrollerClientWidth = [webView stringByEvaluatingJavaScript:scrollerClientWidthScript].intValue;
                return isOverlay ? iframeClientWidth == 200 && scrollerClientWidth == 200 : iframeClientWidth < 200 && scrollerClientWidth < 200;
            }));
        };

        EXPECT_LT([webView stringByEvaluatingJavaScript:iframeClientWidthScript].intValue, 200);
        EXPECT_LT([webView stringByEvaluatingJavaScript:scrollerClientWidthScript].intValue, 200);
        expectScrollbars(@"Legacy");

        changeScrollerStyle(NSScrollerStyleOverlay);
        expectScrollbars(@"Overlay");

        changeScrollerStyle(NSScrollerStyleLegacy);
        expectScrollbars(@"Legacy again");
    }

    // Put anything that observed the swizzled style back on the real one.
    [NSNotificationCenter.defaultCenter postNotificationName:NSPreferredScrollerStyleDidChangeNotification object:nil];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
