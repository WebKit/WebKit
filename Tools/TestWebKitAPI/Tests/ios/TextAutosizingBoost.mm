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

#include "config.h"

#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>

#if WK_API_ENABLED && PLATFORM(IOS_FAMILY)

TEST(TextAutosizingBoost, ChangeAutosizingBoostAtRuntime)
{
    static NSString *testMarkup = @"<meta name='viewport' content='width=device-width'><body style='margin: 0'><span id='top'>Hello world</span><br><span id='bottom'>Goodbye world</span></body>";

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 960, 360)]);
    [webView synchronouslyLoadHTMLString:testMarkup];
    CGSize regularSize {
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().width"] floatValue]),
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().height"] floatValue])
    };

    [webView configuration].preferences._shouldEnableTextAutosizingBoost = YES;
    [webView waitForNextPresentationUpdate];
    CGSize boostedSize {
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().width"] floatValue]),
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().height"] floatValue])
    };

    EXPECT_EQ(125, regularSize.width);
    EXPECT_EQ(30, regularSize.height);
    EXPECT_EQ(159, boostedSize.width);
    EXPECT_EQ(38, boostedSize.height);
}

#endif // WK_API_ENABLED && PLATFORM(IOS_FAMILY)
