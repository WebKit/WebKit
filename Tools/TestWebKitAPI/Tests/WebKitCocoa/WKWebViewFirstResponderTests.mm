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

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

#if PLATFORM(IOS_FAMILY)

@interface FirstResponderTestingView : TestWKWebView
@property (nonatomic, readonly) UITextField *inputField;
@end

@implementation FirstResponderTestingView {
    RetainPtr<UITextField> _inputField;
    RetainPtr<TestInputDelegate> _inputDelegate;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    bool doneFocusing = false;
    _inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [_inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) {
        doneFocusing = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    self._inputDelegate = _inputDelegate.get();
    _inputField = adoptNS([[UITextField alloc] initWithFrame:CGRectMake(0, 0, CGRectGetWidth(frame), 44)]);
    [self.scrollView addSubview:_inputField.get()];
    [self synchronouslyLoadHTMLString:@"<body contenteditable><script>document.body.focus()</script>"];
    TestWebKitAPI::Util::run(&doneFocusing);
    return self;
}

- (UITextField *)inputField
{
    return _inputField.get();
}

@end

namespace TestWebKitAPI {

TEST(WKWebViewFirstResponderTests, ContentViewIsFirstResponder)
{
    auto webView = adoptNS([[FirstResponderTestingView alloc] init]);
    EXPECT_FALSE([webView isFirstResponder]);
    EXPECT_TRUE([webView _contentViewIsFirstResponder]);
    EXPECT_FALSE([webView inputField].isFirstResponder);

    [[webView inputField] becomeFirstResponder];
    EXPECT_FALSE([webView isFirstResponder]);
    EXPECT_FALSE([webView _contentViewIsFirstResponder]);
    EXPECT_TRUE([webView inputField].isFirstResponder);

    [webView becomeFirstResponder];
    EXPECT_FALSE([webView isFirstResponder]);
    EXPECT_TRUE([webView _contentViewIsFirstResponder]);
    EXPECT_FALSE([webView inputField].isFirstResponder);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
