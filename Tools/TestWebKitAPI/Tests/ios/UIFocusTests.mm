/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "TestWKWebView.h"
#import <wtf/Optional.h>
#import <wtf/RetainPtr.h>

@interface UIFocusTestWKWebView : TestWKWebView
@property (nonatomic) Optional<BOOL> overrideCanBecomeFocused;
@end

@implementation UIFocusTestWKWebView

- (BOOL)canBecomeFocused
{
    return self.overrideCanBecomeFocused.valueOr([super canBecomeFocused]);
}

@end

namespace TestWebKitAPI {

TEST(UIFocusTests, OverrideCanBecomeFocused)
{
    auto webView = adoptNS([[UIFocusTestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"simple-form"];
    EXPECT_TRUE([webView textInputContentView].canBecomeFocused);

    [webView setOverrideCanBecomeFocused:NO];
    EXPECT_FALSE([webView textInputContentView].canBecomeFocused);

    [webView setOverrideCanBecomeFocused:YES];
    EXPECT_TRUE([webView textInputContentView].canBecomeFocused);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
