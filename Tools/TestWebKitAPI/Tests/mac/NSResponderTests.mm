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

#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

TEST(NSResponderTests, ValidRequestorForReturnTypes)
{
    auto webView = adoptNS([[TestWKWebView alloc] init]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body)"];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(webView.get(), [webView validRequestorForSendType:nil returnType:(__bridge NSString *)kUTTypePNG]);
    EXPECT_NULL([webView validRequestorForSendType:nil returnType:(__bridge NSString *)kUTTypeAppleScript]);
}

} // namespace TestWebKitAPI
