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

#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/text/WTFString.h>

@interface TestWKWebView (EnterKeyHintTests)
- (void)test:(NSString *)querySelector enterKeyHint:(NSString *)enterKeyHint returnKeyType:(UIReturnKeyType)returnKeyType;
@end

@implementation TestWKWebView (EnterKeyHintTests)

- (void)test:(NSString *)querySelector enterKeyHint:(NSString *)enterKeyHint returnKeyType:(UIReturnKeyType)returnKeyType
{
    [self stringByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('%@').enterKeyHint = '%@'", querySelector, enterKeyHint]];
    [self evaluateJavaScriptAndWaitForInputSessionToChange:[NSString stringWithFormat:@"document.querySelector('%@').focus()", querySelector]];
    EXPECT_EQ(returnKeyType, [self textInputContentView].textInputTraits.returnKeyType);
    [self evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
}

@end

namespace TestWebKitAPI {

using EnterKeyHintTestCase = std::pair<RetainPtr<NSString>, UIReturnKeyType>;

Vector<EnterKeyHintTestCase> enterKeyHintTestCases(UIReturnKeyType fallbackReturnKeyType = UIReturnKeyDefault)
{
    return {
        { @"", fallbackReturnKeyType },
        { @"enter", UIReturnKeyDefault },
        { @"done", UIReturnKeyDone },
        { @"go", UIReturnKeyGo },
        { @"next", UIReturnKeyNext },
        { @"previous", fallbackReturnKeyType },
        { @"search", UIReturnKeySearch },
        { @"send", UIReturnKeySend }
    };
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestInputDelegate>> createWebViewAndInputDelegateForTesting()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    return { WTFMove(webView), WTFMove(inputDelegate) };
}

TEST(EnterKeyHintTests, EnterKeyHintInContentEditableElement)
{
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestInputDelegate> inputDelegate;
    std::tie(webView, inputDelegate) = createWebViewAndInputDelegateForTesting();
    [webView synchronouslyLoadHTMLString:@"<div contenteditable></div>"];
    for (auto& test : enterKeyHintTestCases())
        [webView test:@"div" enterKeyHint:test.first.get() returnKeyType:test.second];
}

TEST(EnterKeyHintTests, EnterKeyHintInTextInput)
{
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestInputDelegate> inputDelegate;
    std::tie(webView, inputDelegate) = createWebViewAndInputDelegateForTesting();
    [webView synchronouslyLoadHTMLString:@""
        "<form action='#' method='post'>"
        "    <input id='one' name='foo' />"
        "    <input id='two' type='search' name='bar' />"
        "    <input type='submit' value='test' />"
        "</form>"
        "<input id='three' />"];

    for (auto& test : enterKeyHintTestCases(UIReturnKeyGo))
        [webView test:@"#one" enterKeyHint:test.first.get() returnKeyType:test.second];

    for (auto& test : enterKeyHintTestCases(UIReturnKeySearch))
        [webView test:@"#two" enterKeyHint:test.first.get() returnKeyType:test.second];

    for (auto& test : enterKeyHintTestCases())
        [webView test:@"#three" enterKeyHint:test.first.get() returnKeyType:test.second];
}

TEST(EnterKeyHintTests, EnterKeyHintInTextArea)
{
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestInputDelegate> inputDelegate;
    std::tie(webView, inputDelegate) = createWebViewAndInputDelegateForTesting();
    [webView synchronouslyLoadHTMLString:@"<textarea></textarea>"];
    for (auto& test : enterKeyHintTestCases())
        [webView test:@"textarea" enterKeyHint:test.first.get() returnKeyType:test.second];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
