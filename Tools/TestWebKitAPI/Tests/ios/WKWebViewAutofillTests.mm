/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

#import "ClassMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/BlockPtr.h>

typedef UIView <UITextInputTraits_Private_Proposed_SPI_34583628> AutofillInputView;

@interface AutofillTestView : TestWKWebView
@end

@implementation AutofillTestView {
    RetainPtr<TestInputDelegate> _testDelegate;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _testDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [_testDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    self._inputDelegate = _testDelegate.get();
    return self;
}

- (AutofillInputView *)_autofillInputView
{
    return (AutofillInputView *)self.textInputContentView;
}

- (BOOL)textInputHasAutofillContext
{
    NSURL *url = [self._autofillInputView._autofillContext objectForKey:@"_WebViewURL"];
    if (![url isKindOfClass:[NSURL class]])
        return NO;

    EXPECT_WK_STREQ([self stringByEvaluatingJavaScript:@"document.URL"], url.absoluteString);
    return YES;
}

@end

namespace TestWebKitAPI {

TEST(WKWebViewAutofillTests, UsernameAndPasswordField)
{
    auto webView = adoptNS([[AutofillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"user.focus()"];
    EXPECT_TRUE([webView textInputHasAutofillContext]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_TRUE([webView textInputHasAutofillContext]);

    auto credentialSuggestion = [UITextAutofillSuggestion autofillSuggestionWithUsername:@"frederik" password:@"famos"];
    [[webView _autofillInputView] insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ("famos", [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);
}

TEST(WKWebViewAutofillTests, UsernameAndPasswordFieldSeparatedByRadioButton)
{
    auto webView = adoptNS([[AutofillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input type='radio' name='radio_button' value='radio'><input id='password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"user.focus()"];
    EXPECT_TRUE([webView textInputHasAutofillContext]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_TRUE([webView textInputHasAutofillContext]);

    auto credentialSuggestion = [UITextAutofillSuggestion autofillSuggestionWithUsername:@"frederik" password:@"famos"];
    [[webView _autofillInputView] insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ("frederik", [webView stringByEvaluatingJavaScript:@"user.value"]);
    EXPECT_WK_STREQ("famos", [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);
}

TEST(WKWebViewAutofillTests, TwoTextFields)
{
    auto webView = adoptNS([[AutofillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='text1' type='email'><input id='text2' type='text'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"text1.focus()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"text2.focus()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);
}

TEST(WKWebViewAutofillTests, StandalonePasswordField)
{
    auto webView = adoptNS([[AutofillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_TRUE([webView textInputHasAutofillContext]);

    auto credentialSuggestion = [UITextAutofillSuggestion autofillSuggestionWithUsername:@"frederik" password:@"famos"];
    [[webView _autofillInputView] insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ("famos", [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);
}

TEST(WKWebViewAutofillTests, StandaloneTextField)
{
    auto webView = adoptNS([[AutofillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='textfield' type='text'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"textfield.focus()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);
}

TEST(WKWebViewAutofillTests, AccountCreationPage)
{
    auto webView = adoptNS([[AutofillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'><input id='confirm_password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"user.focus()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"confirm_password.focus()"];
    EXPECT_FALSE([webView textInputHasAutofillContext]);
}

static BOOL overrideIsInHardwareKeyboardMode()
{
    return NO;
}

TEST(WKWebViewAutofillTests, AutofillRequiresInputSession)
{
    ClassMethodSwizzler swizzler([UIKeyboard class], @selector(isInHardwareKeyboardMode), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));

    bool done = false;
    auto webView = adoptNS([[AutofillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [(TestInputDelegate *)[webView _inputDelegate] setFocusStartsInputSessionPolicyHandler:[&done] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        done = true;
        return _WKFocusStartsInputSessionPolicyAuto;
    }];
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'>"];
    [webView stringByEvaluatingJavaScript:@"user.focus()"];
    Util::run(&done);

    EXPECT_FALSE([webView textInputHasAutofillContext]);
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS_FAMILY)
