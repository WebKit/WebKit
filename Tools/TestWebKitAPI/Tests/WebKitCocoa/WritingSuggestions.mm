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

// FIXME: Get these tests to run on other platforms.
#if PLATFORM(IOS_FAMILY) && ENABLE(WRITING_SUGGESTIONS)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestResourceLoadDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/_WKResourceLoadInfo.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>

@interface WritingSuggestionsWebAPIWKWebView : TestWKWebView

- (void)focusElementAndEnsureEditorStateUpdate:(NSString *)element;

@end

@implementation WritingSuggestionsWebAPIWKWebView {
    RetainPtr<TestInputDelegate> _inputDelegate;
}

- (instancetype)initWithHTMLString:(NSString *)string
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    if (!(self = [super initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]))
        return nil;

    _inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [_inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [self _setInputDelegate:_inputDelegate.get()];

    [self synchronouslyLoadHTMLString:string];

    return self;
}

- (void)focusElementAndEnsureEditorStateUpdate:(NSString *)element
{
    NSString *script = [NSString stringWithFormat:@"%@.focus()", element];
    [self evaluateJavaScriptAndWaitForInputSessionToChange:script];
    [self waitForNextPresentationUpdate];
}

@end

TEST(WritingSuggestionsWebAPI, DefaultState)
{
    auto webView = adoptNS([[WritingSuggestionsWebAPIWKWebView alloc] initWithHTMLString:@"<body><div id='div' contenteditable>This is some text.</div></body>"]);
    [webView focusElementAndEnsureEditorStateUpdate:@"document.getElementById('div')"];

    EXPECT_EQ(UITextInlinePredictionTypeDefault, [webView effectiveTextInputTraits].inlinePredictionType);
}

TEST(WritingSuggestionsWebAPI, DefaultStateWithInput)
{
    auto webView = adoptNS([[WritingSuggestionsWebAPIWKWebView alloc] initWithHTMLString:@"<body><input id='input' type='text'></input></body>"]);
    [webView focusElementAndEnsureEditorStateUpdate:@"document.getElementById('input')"];

    EXPECT_EQ(UITextInlinePredictionTypeDefault, [webView effectiveTextInputTraits].inlinePredictionType);
}

TEST(WritingSuggestionsWebAPI, ExplicitlyEnabled)
{
    auto webView = adoptNS([[WritingSuggestionsWebAPIWKWebView alloc] initWithHTMLString:@"<body><div id='div' contenteditable writingsuggestions='true'>This is some text.</div></body>"]);
    [webView focusElementAndEnsureEditorStateUpdate:@"document.getElementById('div')"];

    EXPECT_EQ(UITextInlinePredictionTypeDefault, [webView effectiveTextInputTraits].inlinePredictionType);
}

TEST(WritingSuggestionsWebAPI, ExplicitlyDisabled)
{
    auto webView = adoptNS([[WritingSuggestionsWebAPIWKWebView alloc] initWithHTMLString:@"<body><div id='div' contenteditable writingsuggestions='false'>This is some text.</div></body>"]);
    [webView focusElementAndEnsureEditorStateUpdate:@"document.getElementById('div')"];

    EXPECT_EQ(UITextInlinePredictionTypeNo, [webView effectiveTextInputTraits].inlinePredictionType);
}

TEST(WritingSuggestionsWebAPI, ExplicitlyDisabledOnParent)
{
    auto webView = adoptNS([[WritingSuggestionsWebAPIWKWebView alloc] initWithHTMLString:@"<body><div writingsuggestions='false'><div id='div' contenteditable>This is some text.</div></div></body>"]);
    [webView focusElementAndEnsureEditorStateUpdate:@"document.getElementById('div')"];

    EXPECT_EQ(UITextInlinePredictionTypeNo, [webView effectiveTextInputTraits].inlinePredictionType);
}

TEST(WritingSuggestionsWebAPI, DefaultStateWithDisabledAutocomplete)
{
    auto webView = adoptNS([[WritingSuggestionsWebAPIWKWebView alloc] initWithHTMLString:@"<body><input id='input' type='text' autocomplete='off'></input></body>"]);
    [webView focusElementAndEnsureEditorStateUpdate:@"document.getElementById('input')"];

    EXPECT_EQ(UITextInlinePredictionTypeNo, [webView effectiveTextInputTraits].inlinePredictionType);
}

#endif
