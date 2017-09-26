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

#if WK_API_ENABLED && PLATFORM(IOS) && USE(APPLE_INTERNAL_SDK)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"

#import <UIKit/UITextInputTraits_Private.h>
#import <UIKit/UITextInput_Private.h>

// FIXME 34583628: Simplyfy this once the UIKit work is available in the build.
#if __has_include(<UIKit/UIKeyboardLoginCredentialsSuggestion.h>)
#import <UIKit/UIKeyboardLoginCredentialsSuggestion.h>
#else
#import <UIKit/UITextSuggestion.h>
@interface UIKeyboardLoginCredentialsSuggestion : UITextSuggestion

@property (nonatomic, assign) NSString *username;
@property (nonatomic, assign) NSString *password;

@end
#endif

static UIKeyboardLoginCredentialsSuggestion *createUIKeyboardLoginCredentialsSuggestion()
{
    return [[NSClassFromString(@"UIKeyboardLoginCredentialsSuggestion") new] autorelease];
}

typedef UIView <UITextInputPrivate, UITextInputTraits_Private, UITextInputTraits_Private_Staging_34583628>  AutofillCandidateView;

@implementation WKWebView (AutofillTestHelpers)

- (AutofillCandidateView *)_privateTextInput
{
    return (AutofillCandidateView *)[self valueForKey:@"_currentContentView"];
}

@end

namespace TestWebKitAPI {

TEST(WKWebViewAutofillTests, UsernameAndPasswordField)
{
    TestWKWebView *webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)];
    AutofillCandidateView *contentView = [webView _privateTextInput];
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'>"];
    [webView stringByEvaluatingJavaScript:@"user.focus()"];
    EXPECT_TRUE(contentView.acceptsAutofilledLoginCredentials);

    [webView stringByEvaluatingJavaScript:@"password.focus()"];
    EXPECT_TRUE(contentView.acceptsAutofilledLoginCredentials);

    UIKeyboardLoginCredentialsSuggestion *credentialSuggestion = createUIKeyboardLoginCredentialsSuggestion();
    credentialSuggestion.username = @"frederik";
    credentialSuggestion.password = @"famos";

    [contentView insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ(credentialSuggestion.username, [webView stringByEvaluatingJavaScript:@"user.value"]);
    EXPECT_WK_STREQ(credentialSuggestion.password, [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView release];
}

TEST(WKWebViewAutofillTests, UsernameAndPasswordFieldSeparatedByRadioButton)
{
    TestWKWebView *webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)];
    AutofillCandidateView *contentView = [webView _privateTextInput];
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input type='radio' name='radio_button' value='radio'><input id='password' type='password'>"];
    [webView stringByEvaluatingJavaScript:@"user.focus()"];
    EXPECT_TRUE(contentView.acceptsAutofilledLoginCredentials);

    [webView stringByEvaluatingJavaScript:@"password.focus()"];
    EXPECT_TRUE(contentView.acceptsAutofilledLoginCredentials);

    UIKeyboardLoginCredentialsSuggestion *credentialSuggestion = createUIKeyboardLoginCredentialsSuggestion();
    credentialSuggestion.username = @"frederik";
    credentialSuggestion.password = @"famos";

    [contentView insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ(credentialSuggestion.username, [webView stringByEvaluatingJavaScript:@"user.value"]);
    EXPECT_WK_STREQ(credentialSuggestion.password, [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView release];
}

TEST(WKWebViewAutofillTests, TwoTextFields)
{
    TestWKWebView *webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)];
    AutofillCandidateView *contentView = [webView _privateTextInput];
    [webView synchronouslyLoadHTMLString:@"<input id='text1' type='email'><input id='text2' type='text'>"];
    [webView stringByEvaluatingJavaScript:@"text1.focus()"];
    EXPECT_FALSE(contentView.acceptsAutofilledLoginCredentials);

    [webView stringByEvaluatingJavaScript:@"text2.focus()"];
    EXPECT_FALSE(contentView.acceptsAutofilledLoginCredentials);
}

TEST(WKWebViewAutofillTests, StandalonePasswordField)
{
    TestWKWebView *webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)];
    AutofillCandidateView *contentView = [webView _privateTextInput];
    [webView synchronouslyLoadHTMLString:@"<input id='password' type='password'>"];
    [webView stringByEvaluatingJavaScript:@"password.focus()"];
    EXPECT_TRUE(contentView.acceptsAutofilledLoginCredentials);

    UIKeyboardLoginCredentialsSuggestion *credentialSuggestion = createUIKeyboardLoginCredentialsSuggestion();
    credentialSuggestion.username = @"frederik";
    credentialSuggestion.password = @"famos";

    [contentView insertTextSuggestion:credentialSuggestion];

    EXPECT_WK_STREQ(credentialSuggestion.password, [webView stringByEvaluatingJavaScript:@"password.value"]);

    [webView release];
}

TEST(WKWebViewAutofillTests, StandaloneTextField)
{
    TestWKWebView *webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)];
    AutofillCandidateView *contentView = [webView _privateTextInput];
    [webView synchronouslyLoadHTMLString:@"<input id='textfield' type='text'>"];
    [webView stringByEvaluatingJavaScript:@"textfield.focus()"];
    EXPECT_FALSE(contentView.acceptsAutofilledLoginCredentials);

    [webView release];
}

TEST(WKWebViewAutofillTests, AccountCreationPage)
{
    TestWKWebView *webView = [[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)];
    AutofillCandidateView *contentView = [webView _privateTextInput];
    [webView synchronouslyLoadHTMLString:@"<input id='user' type='email'><input id='password' type='password'><input id='confirm_password' type='password'>"];
    [webView stringByEvaluatingJavaScript:@"user.focus()"];
    EXPECT_FALSE(contentView.acceptsAutofilledLoginCredentials);

    [webView stringByEvaluatingJavaScript:@"password.focus()"];
    EXPECT_FALSE(contentView.acceptsAutofilledLoginCredentials);

    [webView stringByEvaluatingJavaScript:@"confirm_password.focus()"];
    EXPECT_FALSE(contentView.acceptsAutofilledLoginCredentials);

    [webView release];
}


} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS) && USE(APPLE_INTERNAL_SDK)
