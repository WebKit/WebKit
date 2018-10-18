/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "Test.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/BlockPtr.h>

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestInputDelegate>> webViewForTestingFocusPreservation(void(^focusHandler)(id <_WKFocusedElementInfo>))
{
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&, focusHandler = makeBlockPtr(focusHandler)] (WKWebView *, id<_WKFocusedElementInfo> info) {
        focusHandler(info);
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input></input><select><option selected>foo</option><option>bar</option></select>"];
    return { webView, inputDelegate };
}

namespace TestWebKitAPI {

TEST(FocusPreservationTests, PreserveAndRestoreFocus)
{
    bool inputFocused = false;
    auto webViewAndDelegate = webViewForTestingFocusPreservation([&inputFocused] (id <_WKFocusedElementInfo> info) {
        inputFocused = true;
    });

    TestWKWebView *webView = webViewAndDelegate.first.get();
    [webView evaluateJavaScript:@"document.querySelector('input').focus()" completionHandler:nil];
    Util::run(&inputFocused);

    NSUUID *focusToken = NSUUID.UUID;
    [webView.textInputContentView _preserveFocusWithToken:focusToken destructively:YES];

    EXPECT_TRUE([webView resignFirstResponder]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.activeElement == document.querySelector('input')"].boolValue);

    [webView.textInputContentView _restoreFocusWithToken:focusToken];
    EXPECT_TRUE([webView becomeFirstResponder]);
}

TEST(FocusPreservationTests, ChangingFocusedNodeResetsFocusPreservationState)
{
    bool inputFocused = false;
    bool selectFocused = false;
    auto webViewAndDelegate = webViewForTestingFocusPreservation([&inputFocused, &selectFocused] (id <_WKFocusedElementInfo> info) {
        if (info.type == WKInputTypeSelect)
            selectFocused = true;
        else
            inputFocused = true;
    });

    TestWKWebView *webView = webViewAndDelegate.first.get();
    [webView evaluateJavaScript:@"document.querySelector('input').focus()" completionHandler:nil];
    Util::run(&inputFocused);

    NSUUID *focusToken = NSUUID.UUID;
    [webView.textInputContentView _preserveFocusWithToken:focusToken destructively:YES];

    [webView evaluateJavaScript:@"document.querySelector('select').focus()" completionHandler:nil];
    Util::run(&selectFocused);

    EXPECT_NOT_NULL(webView.textInputContentView.inputView);
    [webView selectFormAccessoryPickerRow:1];
    EXPECT_TRUE([webView resignFirstResponder]);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"document.activeElement == document.querySelector('select')"].boolValue);
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"document.querySelector('select').selectedIndex"].intValue);

    [webView.textInputContentView _restoreFocusWithToken:focusToken];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
