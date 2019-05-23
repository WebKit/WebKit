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

#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <CoreText/CoreText.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

TEST(WebKit, TextWidth)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"TextWidth" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    [webView _test_waitForDidFinishNavigation];
    
    __block bool didEvaluateJavaScript = false;
    __block float webKitWidth;
    [webView evaluateJavaScript:@"runTest1()" completionHandler:^(id value, NSError *error) {
        webKitWidth = [(NSNumber *)value floatValue];
        didEvaluateJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&didEvaluateJavaScript);

    auto font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 24, static_cast<CFStringRef>(@"en-US")));
    // Use CFAttributedString so we don't have to deal with NSFont / UIFont and have this code be platform-dependent.
    CFTypeRef keys[] = { kCTFontAttributeName };
    CFTypeRef values[] = { font.get() };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto attributedString = adoptCF(CFAttributedStringCreate(kCFAllocatorDefault, CFSTR("This is a test string"), attributes.get()));
    auto line = adoptCF(CTLineCreateWithAttributedString(static_cast<CFAttributedStringRef>(attributedString)));
    double coreTextWidth = CTLineGetTypographicBounds(line.get(), nullptr, nullptr, nullptr);

    EXPECT_NEAR(webKitWidth, coreTextWidth, 3);
}
