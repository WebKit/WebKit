/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfiguration.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(DataDetectorTests, LoadWKWebViewWithDataDetectorTypePhoneNumber)
{
    NSString *const phoneNumber = @"(555) 867-5309";

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().dataDetectorTypes = WKDataDetectorTypePhoneNumber;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<!DOCTYPE><html><head></head><body><p>Call Jenny at %@</p></body></html>", phoneNumber]];

    // Ensure that the phone number is linked by Data Detectors.
    NSString *linkCount = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('a').length"];
    EXPECT_EQ(1, linkCount.intValue);
    NSString *linkText = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('a')[0].innerText"];
    EXPECT_WK_STREQ(phoneNumber, linkText);
    NSString *linkURL = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('a')[0].href"];
    NSString *expectedLinkURL = [NSString stringWithFormat:@"tel:%@", [phoneNumber stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet whitespaceCharacterSet].invertedSet]];
    EXPECT_WK_STREQ(expectedLinkURL, linkURL);
}

} // namespace TestWebKitAPI

#endif // ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
