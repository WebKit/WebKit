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

#if PLATFORM(IOS_FAMILY)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "TestInputDelegate.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTestingIOS.h>
#import <wtf/RetainPtr.h>

constexpr CGFloat keyboardHeight = 300;

static double viewportUnitLength(RetainPtr<TestWKWebView>& webView, NSString *viewportUnit)
{
    NSString *script = [NSString stringWithFormat:@"document.getElementById('%@').getBoundingClientRect().height", viewportUnit];
    return [(NSNumber *)[webView objectByEvaluatingJavaScript:script] doubleValue];
}

static void setInteractiveWidget(RetainPtr<TestWKWebView>& webView, NSString *value)
{
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('meta[name=viewport]').content = 'width=device-width, initial-scale=1, interactive-widget=%@'", value]];
}

static void simulateKeyboardOfHeight(RetainPtr<TestWKWebView>& webView, CGFloat height)
{
    RetainPtr window = [webView window];
    CGRect frameInWindow = [webView convertRect:CGRectMake(0, CGRectGetHeight([webView bounds]) - height, CGRectGetWidth([webView bounds]), height) toView:window];
    CGRect frameInScreen = [window convertRect:frameInWindow toCoordinateSpace:[window screen].coordinateSpace];

    [NSNotificationCenter.defaultCenter postNotificationName:UIKeyboardDidChangeFrameNotification object:nil userInfo:@{
        UIKeyboardFrameEndUserInfoKey: [NSValue valueWithCGRect:frameInScreen],
        UIKeyboardIsLocalUserInfoKey: @YES,
    }];

    EXPECT_FALSE(CGRectIsEmpty([webView _inputViewBoundsInWindow]));
    [webView waitForNextPresentationUpdate];
}

static void focusInput(RetainPtr<TestWKWebView>& webView, RetainPtr<TestInputDelegate>& inputDelegate)
{
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate];
    [webView focusInWindow];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('input').focus()"];
    EXPECT_WK_STREQ("input", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);
}

TEST(InteractiveWidget, ViewportUnitsOverlaysContent)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);

    [webView synchronouslyLoadTestPageNamed:@"InteractiveWidgetViewportUnits"];
    setInteractiveWidget(webView, @"overlays-content");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));

    focusInput(webView, inputDelegate);
    simulateKeyboardOfHeight(webView, keyboardHeight);

    double dvh = viewportUnitLength(webView, @"dvh");
    EXPECT_FLOAT_EQ(500, dvh);
    EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"lvh"));
}

TEST(InteractiveWidget, ViewportUnitsResizesContent)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);

    [webView synchronouslyLoadTestPageNamed:@"InteractiveWidgetViewportUnits"];
    setInteractiveWidget(webView, @"resizes-content");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));

    focusInput(webView, inputDelegate);
    simulateKeyboardOfHeight(webView, keyboardHeight);

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500 - keyboardHeight, viewportUnitLength(webView, @"vh"));

    double dvh = viewportUnitLength(webView, @"dvh");
    EXPECT_FLOAT_EQ(500 - keyboardHeight, dvh);
    EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"lvh"));
}

TEST(InteractiveWidget, ViewportUnitsResizesContentWithOverriddenLayoutParameters)
{
    constexpr CGFloat viewHeight = 500;
    constexpr CGFloat topBarHeight = 50;
    constexpr CGFloat bottomBarHeight = 44;
    constexpr CGFloat minimumUnobscuredHeight = viewHeight - topBarHeight - bottomBarHeight;
    constexpr CGFloat maximumUnobscuredHeight = viewHeight - topBarHeight;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);

    [webView _setObscuredInsets:UIEdgeInsetsMake(topBarHeight, 0, bottomBarHeight, 0)];
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(320, minimumUnobscuredHeight) minimumUnobscuredSizeOverride:CGSizeMake(320, minimumUnobscuredHeight) maximumUnobscuredSizeOverride:CGSizeMake(320, maximumUnobscuredHeight)];

    [webView synchronouslyLoadTestPageNamed:@"InteractiveWidgetViewportUnits"];
    setInteractiveWidget(webView, @"resizes-content");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(maximumUnobscuredHeight, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(minimumUnobscuredHeight, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(maximumUnobscuredHeight, viewportUnitLength(webView, @"lvh"));

    focusInput(webView, inputDelegate);
    simulateKeyboardOfHeight(webView, keyboardHeight);

    constexpr CGFloat resizedHeight = viewHeight - keyboardHeight - topBarHeight;
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(resizedHeight, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(resizedHeight, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(resizedHeight, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(resizedHeight, viewportUnitLength(webView, @"dvh"));
}

#endif // PLATFORM(IOS_FAMILY)
