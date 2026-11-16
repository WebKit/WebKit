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

#if PLATFORM(IOS)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/ios/UserInterfaceSwizzler.h"
#import "TestInputDelegate.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTestingIOS.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/RetainPtr.h>

@interface ZoomToFocusedElementScrollViewDelegate : NSObject <UIScrollViewDelegate>
@property (nonatomic, readonly) BOOL didEndZooming;
@end

@implementation ZoomToFocusedElementScrollViewDelegate

- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    _didEndZooming = YES;
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebViewWithSizeClasses(UIUserInterfaceSizeClass horizontalSizeClass, UIUserInterfaceSizeClass verticalSizeClass, CGSize viewSize, TestInputDelegate *inputDelegate)
{
    PAL::updateCurrentUserInterfaceIdiom();

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, viewSize.width, viewSize.height)]);
    [webView traitOverrides].horizontalSizeClass = horizontalSizeClass;
    [webView traitOverrides].verticalSizeClass = verticalSizeClass;
    [webView updateTraitsIfNeeded];
    EXPECT_EQ(horizontalSizeClass, [webView textInputContentView].traitCollection.horizontalSizeClass);
    EXPECT_EQ(verticalSizeClass, [webView textInputContentView].traitCollection.verticalSizeClass);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate];
    return webView;
}

static bool focusedInputIsVisible(TestWKWebView *webView)
{
    CGRect inputRect = [webView convertRect:[webView _focusedElementInteractionRect] fromView:[webView textInputContentView]];
    return CGRectContainsRect([webView bounds], inputRect);
}

static void testZoomAfterFocusingInput(UIUserInterfaceSizeClass horizontalSizeClass, UIUserInterfaceSizeClass verticalSizeClass, CGSize viewSize)
{
    IPhoneUserInterfaceSwizzler iPhoneUserInterface;

    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    RetainPtr webView = createWebViewWithSizeClasses(horizontalSizeClass, verticalSizeClass, viewSize, inputDelegate);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<body style='margin: 0; height: 200vh;'>"
        "<input id='input' style='position: absolute; right: 10px; top: calc(100vh - 50px); width: 150px; height: 30px; font-size: 12px;'>"
        "</body>"];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(1, [webView scrollView].zoomScale);

    RetainPtr scrollViewDelegate = adoptNS([ZoomToFocusedElementScrollViewDelegate new]);
    [webView scrollView].delegate = scrollViewDelegate;

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"input.focus()"];
    EXPECT_TRUE(Util::waitFor([&] {
        return [scrollViewDelegate didEndZooming];
    }));
    [webView waitForNextPresentationUpdate];

    // Focused inputs are zoomed so that their text appears at a font size of 16px.
    EXPECT_NEAR(16. / 12, [webView scrollView].zoomScale, 0.01);
    EXPECT_TRUE(focusedInputIsVisible(webView));
}

template<UIUserInterfaceIdiom idiom>
static void testNoZoomAfterFocusingInput(UIUserInterfaceSizeClass horizontalSizeClass, UIUserInterfaceSizeClass verticalSizeClass, CGSize viewSize)
{
    UserInterfaceSwizzler<idiom> userInterface;

    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    RetainPtr webView = createWebViewWithSizeClasses(horizontalSizeClass, verticalSizeClass, viewSize, inputDelegate);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<body style='margin: 0; height: 300vh;'>"
        "<input id='input' style='position: absolute; left: 10px; top: 150vh; width: 150px; height: 30px; font-size: 12px;'>"
        "</body>"];
    [webView waitForNextPresentationUpdate];

    auto initialZoomScale = [webView scrollView].zoomScale;
    auto initialContentOffset = [webView scrollView].contentOffset;

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"input.focus()"];
    EXPECT_TRUE(Util::waitFor([&] {
        return [webView scrollView].contentOffset.y > initialContentOffset.y;
    }));
    [webView waitForNextPresentationUpdate];

    EXPECT_NEAR(initialZoomScale, [webView scrollView].zoomScale, 0.01);
    EXPECT_TRUE(focusedInputIsVisible(webView));
}

TEST(ZoomToFocusedElement, PhoneIdiomCompactWidthRegularHeight)
{
    testZoomAfterFocusingInput(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassRegular, CGSizeMake(390, 844));
}

TEST(ZoomToFocusedElement, PhoneIdiomCompactWidthCompactHeight)
{
    testZoomAfterFocusingInput(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassCompact, CGSizeMake(844, 390));
}

TEST(ZoomToFocusedElement, PhoneIdiomRegularWidthCompactHeight)
{
    testZoomAfterFocusingInput(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassCompact, CGSizeMake(932, 430));
}

TEST(ZoomToFocusedElement, PhoneIdiomRegularWidthRegularHeight)
{
    testNoZoomAfterFocusingInput<UIUserInterfaceIdiomPhone>(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassRegular, CGSizeMake(820, 1180));
}

TEST(ZoomToFocusedElement, PadIdiomCompactWidthRegularHeight)
{
    testNoZoomAfterFocusingInput<UIUserInterfaceIdiomPad>(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassRegular, CGSizeMake(390, 844));
}

TEST(ZoomToFocusedElement, PadIdiomCompactWidthCompactHeight)
{
    testNoZoomAfterFocusingInput<UIUserInterfaceIdiomPad>(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassCompact, CGSizeMake(844, 390));
}

TEST(ZoomToFocusedElement, PadIdiomRegularWidthCompactHeight)
{
    testNoZoomAfterFocusingInput<UIUserInterfaceIdiomPad>(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassCompact, CGSizeMake(932, 430));
}

TEST(ZoomToFocusedElement, PadIdiomRegularWidthRegularHeight)
{
    testNoZoomAfterFocusingInput<UIUserInterfaceIdiomPad>(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassRegular, CGSizeMake(820, 1180));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS)
