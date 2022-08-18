/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, GetTextFragmentMatch)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

    auto testTextFragment = ^(NSString *pageContent, NSString *textFragment, NSString *expectedResult) {
        // Load an empty baseURL-less string, otherwise using the same baseURL (modulo the fragment) does a same-document navigation.
        [webView synchronouslyLoadHTMLString:@""];
        [webView synchronouslyLoadHTMLString:pageContent baseURL:[NSURL URLWithString:[@"http://example.com/" stringByAppendingString:textFragment]]];

        __block bool isDone;
        [webView _getTextFragmentMatchWithCompletionHandler:^(NSString *string) {
            EXPECT_WK_STREQ(string, expectedResult);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
    };

    testTextFragment(@"hello world", @"#:~:text=hello%20world", @"hello world");
    testTextFragment(@"<span id='the'>The</span> quick brown fox <span id='jumps'>jumps</span> over the lazy <span id='dog'>dog.</span>", @"#:~:text=quick,jumps", @"quick brown fox jumps");
    testTextFragment(@"a the first match b the second match c", @"#:~:text=a-,the,match,-b", @"the first match");
    testTextFragment(@"a the first match b the second match c", @"#:~:text=b-,the,match,-c", @"the second match");
    testTextFragment(@"no match", @"#:~:text=hello%20world", nil);
}
