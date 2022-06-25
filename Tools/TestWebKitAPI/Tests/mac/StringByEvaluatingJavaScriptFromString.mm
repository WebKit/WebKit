/*
 * Copyright (C) 2007, 2011 Apple Inc. All rights reserved.
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
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKitLegacy, StringByEvaluatingJavaScriptFromString)
{
    // maps expected result <= JavaScript expression
    RetainPtr<NSDictionary> expressions = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
        @"0", @"0",
        @"0", @"'0'",
        @"", @"",
        @"", @"''",
        @"", @"new String()",
        @"", @"new String('0')",
        @"", @"throw 1",
        @"", @"{ }",
        @"", @"[ ]",
        @"", @"//",
        @"", @"a.b.c",
        @"", @"(function() { throw 'error'; })()",
        @"", @"null",
        @"", @"undefined",
        @"true", @"true",
        @"false", @"false",
        @"", @"alert('Should not be result')",
        nil
    ]);

    RetainPtr<WebView> webView  = adoptNS([[WebView alloc] initWithFrame:NSZeroRect frameName:@"" groupName:@""]);

    // Test a nil string
    NSString *result = [webView.get() stringByEvaluatingJavaScriptFromString:nil];
    EXPECT_WK_STREQ(@"", result);

    for (id expression in expressions.get()) {
        NSString *expectedResult = [expressions.get() objectForKey:expression];
        NSString *result = [webView.get() stringByEvaluatingJavaScriptFromString:expression];
        EXPECT_WK_STREQ(expectedResult, result);
    }

    [webView.get() close];
}

} // namespace TestWebKitAPI
