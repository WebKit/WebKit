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

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "ClassMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/BlockPtr.h>

@protocol WKTextInputSuggestionDelegate <UITextInputSuggestionDelegate>
- (NSArray<UITextSuggestion *> *)suggestions;
@end

typedef UIView <UITextInputPrivate> AutoFillInputView;

@interface AutoFillTestView : TestWKWebView
@end

@implementation AutoFillTestView {
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

- (AutoFillInputView *)_autofillInputView
{
    return (AutoFillInputView *)self.textInputContentView;
}

- (BOOL)acceptsAutoFillLoginCredentials
{
    auto context = self._autofillInputView._autofillContext;
    if (!context)
        return NO;

    NSURL *url = context[@"_WebViewURL"];
    EXPECT_TRUE([url isKindOfClass:NSURL.class]);
    EXPECT_WK_STREQ([self stringByEvaluatingJavaScript:@"document.URL"], url.absoluteString);
    return [context[@"_acceptsLoginCredentials"] boolValue];
}

@end

namespace TestWebKitAPI {

TEST(WKWebViewAutoFillTests, UsernameAndPasswordField)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"user.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    auto credentialSuggestion = [UITextAutofillSuggestion autofillSuggestionWithUsername:@"frederik" password:@"famos"];
    [[webView _autofillInputView] insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ("famos", [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);
}

TEST(WKWebViewAutoFillTests, UsernameAndPasswordFieldAcrossShadowBoundaries)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<div id=emailHost></div><div id=passwordHost></div><script>"
        "emailRoot = emailHost.attachShadow({mode: 'closed'}); emailRoot.innerHTML = '<input id=user type=email>';"
        "passwordRoot = passwordHost.attachShadow({mode: 'closed'}); passwordRoot.innerHTML = '<input id=password type=password>';"
        "</script>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"emailRoot.querySelector('input').focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"passwordRoot.querySelector('input').focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    auto credentialSuggestion = [UITextAutofillSuggestion autofillSuggestionWithUsername:@"frederik" password:@"famos"];
    [[webView _autofillInputView] insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ("famos", [webView stringByEvaluatingJavaScript:@"passwordRoot.querySelector('input').value"]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);
}

TEST(WKWebViewAutoFillTests, UsernameAndPasswordFieldSeparatedByRadioButton)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input type='radio' name='radio_button' value='radio'><input id='password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"user.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    auto credentialSuggestion = [UITextAutofillSuggestion autofillSuggestionWithUsername:@"frederik" password:@"famos"];
    [[webView _autofillInputView] insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ("frederik", [webView stringByEvaluatingJavaScript:@"user.value"]);
    EXPECT_WK_STREQ("famos", [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);
}

TEST(WKWebViewAutoFillTests, TwoTextFields)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='text1' type='email'><input id='text2' type='text'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"text1.focus()"];
    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"text2.focus()"];
    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);
}

TEST(WKWebViewAutoFillTests, StandalonePasswordField)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    auto credentialSuggestion = [UITextAutofillSuggestion autofillSuggestionWithUsername:@"frederik" password:@"famos"];
    [[webView _autofillInputView] insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ("famos", [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.activeElement.blur()"];
    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);
}

TEST(WKWebViewAutoFillTests, StandaloneTextField)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='textfield' type='text'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"textfield.focus()"];
    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);
}

TEST(WKWebViewAutoFillTests, AccountCreationPage)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'><input id='confirm_password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"user.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"password.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"confirm_password.focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);
}

TEST(WKWebViewAutoFillTests, AccountCreationPageAcrossShadowBoundaries)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<div id=emailHost></div><div id=passwordHost></div><div id=confirmHost></div><script>"
        "emailRoot = emailHost.attachShadow({mode: 'closed'}); emailRoot.innerHTML = '<input id=user type=email>';"
        "passwordRoot = passwordHost.attachShadow({mode: 'closed'}); passwordRoot.innerHTML = '<input id=password type=password>';"
        "confirmRoot = confirmHost.attachShadow({mode: 'closed'}); confirmRoot.innerHTML = '<input id=confirm_password type=password>';"
        "</script>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"emailRoot.querySelector('input').focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"passwordRoot.querySelector('input').focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"confirmRoot.querySelector('input').focus()"];
    EXPECT_TRUE([webView acceptsAutoFillLoginCredentials]);
}

static BOOL overrideIsInHardwareKeyboardMode()
{
    return NO;
}

TEST(WKWebViewAutoFillTests, AutoFillRequiresInputSession)
{
    ClassMethodSwizzler swizzler([UIKeyboard class], @selector(isInHardwareKeyboardMode), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));

    bool done = false;
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [(TestInputDelegate *)[webView _inputDelegate] setFocusStartsInputSessionPolicyHandler:[&done] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        done = true;
        return _WKFocusStartsInputSessionPolicyAuto;
    }];
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'>"];
    [webView stringByEvaluatingJavaScript:@"user.focus()"];
    Util::run(&done);

    EXPECT_FALSE([webView acceptsAutoFillLoginCredentials]);
}

#if PLATFORM(WATCHOS)

TEST(WKWebViewAutoFillTests, DoNotShowBlankTextSuggestions)
{
    auto webView = adoptNS([[AutoFillTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [(TestInputDelegate *)[webView _inputDelegate] setWillStartInputSessionHandler:[](WKWebView *, id <_WKFormInputSession> session) {
        auto emptySuggestion = adoptNS([[UITextSuggestion alloc] init]);
        [emptySuggestion setDisplayText:@""];
        session.suggestions = @[ emptySuggestion.get() ];
    }];
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"user.focus()"];

    NSArray *suggestions = [(id <WKTextInputSuggestionDelegate>)[[webView textInputContentView] inputDelegate] suggestions];
    EXPECT_EQ(0U, suggestions.count);
}

#endif

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
