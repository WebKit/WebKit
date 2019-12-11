/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>

@interface TestWKWebView (AutocorrectionTests)
- (UIWKAutocorrectionRects *)autocorrectionRectsForString:(NSString *)string;
@end

@implementation TestWKWebView (AutocorrectionTests)

- (UIWKAutocorrectionRects *)autocorrectionRectsForString:(NSString *)string
{
    RetainPtr<UIWKAutocorrectionRects> result;
    bool done = false;
    [self.textInputContentView requestAutocorrectionRectsForString:string withCompletionHandler:[&] (UIWKAutocorrectionRects *rects) {
        result = rects;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

static void checkCGRectIsEqualToCGRectWithLogging(CGRect expected, CGRect observed)
{
    BOOL isEqual = CGRectEqualToRect(expected, observed);
    EXPECT_TRUE(isEqual);
    if (!isEqual)
        NSLog(@"Expected: %@ but observed: %@", NSStringFromCGRect(expected), NSStringFromCGRect(observed));
}

TEST(AutocorrectionTests, FontAtCaretWhenUsingUICTFontTextStyle)
{
    auto webView = adoptNS([[TestWKWebView alloc] init]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='font-size: 16px; font-family: UICTFontTextStyleBody'>Wulk</body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [webView _executeEditCommand:@"MoveToEndOfLine" argument:nil completion:nil];

    auto autocorrectionRects = retainPtr([webView autocorrectionRectsForString:@"Wulk"]);
    checkCGRectIsEqualToCGRectWithLogging(CGRectMake(8, 8, 36, 21), [autocorrectionRects firstRect]);
    checkCGRectIsEqualToCGRectWithLogging(CGRectMake(8, 8, 36, 21), [autocorrectionRects lastRect]);

    auto contentView = [webView textInputContentView];
    UIFont *fontBeforeScaling = [contentView fontForCaretSelection];
    UIFont *size16SystemFont = [UIFont systemFontOfSize:16];
    EXPECT_WK_STREQ(size16SystemFont.fontName, fontBeforeScaling.fontName);
    EXPECT_WK_STREQ(size16SystemFont.familyName, fontBeforeScaling.familyName);
    EXPECT_EQ(16, fontBeforeScaling.pointSize);

    [webView scrollView].zoomScale = 2;
    UIFont *fontAfterScaling = [contentView fontForCaretSelection];
    UIFont *size32SystemFont = [UIFont systemFontOfSize:32];
    EXPECT_WK_STREQ(size32SystemFont.fontName, fontAfterScaling.fontName);
    EXPECT_WK_STREQ(size32SystemFont.familyName, fontAfterScaling.familyName);
    EXPECT_EQ(32, fontAfterScaling.pointSize);
}

TEST(AutocorrectionTests, RequestAutocorrectionContextAfterClosingPage)
{
    auto webView = adoptNS([[TestWKWebView alloc] init]);
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];

    auto contentView = [webView textInputContentView];
    [contentView resignFirstResponder];
    [webView _close];

    bool done = false;
    [contentView requestAutocorrectionContextWithCompletionHandler:[&] (UIWKAutocorrectionContext *) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

#endif // PLATFORM(IOS_FAMILY)
