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
#import <WebKit/WKUIDelegatePrivate.h>
#import <wtf/RetainPtr.h>

@interface UIFocusDelegate : NSObject<WKUIDelegatePrivate>
@property (nonatomic) BOOL canBecomeFocused;
@end

@implementation UIFocusDelegate

- (BOOL)_webViewCanBecomeFocused:(WKWebView *)webView
{
    return self.canBecomeFocused;
}

- (void)_webView:(WKWebView *)webView takeFocus:(_WKFocusDirection)direction
{
}

@end

namespace TestWebKitAPI {

TEST(UIFocusTests, ContentViewCanBecomeFocused)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"simple-form"];

    auto contentView = [webView textInputContentView];
    EXPECT_FALSE(contentView.canBecomeFocused);

    auto delegate = adoptNS([[UIFocusDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [delegate setCanBecomeFocused:YES];
    EXPECT_TRUE(contentView.canBecomeFocused);

    [delegate setCanBecomeFocused:NO];
    EXPECT_FALSE(contentView.canBecomeFocused);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
