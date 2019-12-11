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

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFindConfiguration.h>
#import <WebKit/WKFindResult.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>

NSString *testPage = @"<style>\n"
"@media screen {\n"
".ShowForPrinting{\n"
"    visibility: hidden;\n"
"}\n"
".ShowForLavaLamp{\n"
"    visibility: hidden;\n"
"}\n"
"}\n"
"@media print {\n"
".ShowForScreen{\n"
"    visibility: hidden;\n"
"}\n"
".ShowForLavaLamp{\n"
"    visibility: hidden;\n"
"}\n"
"}\n"
"@media lavalamp {\n"
".ShowForScreen {\n"
"    visibility: hidden;\n"
"}\n"
".ShowForPrinting{\n"
"    visibility: hidden;\n"
"}\n"
"}\n"
"</style>\n"
"<div class='ShowForScreen'>Screen</div>\n"
"<div class='ShowForPrinting'>Print</div>\n"
"<div class='ShowForLavaLamp'>LavaLamp</div>\n";

TEST(WKWebView, MediaType)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:testPage baseURL:nil];

    EXPECT_TRUE(webView.get().mediaType == nil);
    EXPECT_TRUE([[[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] isEqualToString:@"Screen"]);
    
    webView.get().mediaType = @"screen";
    EXPECT_TRUE([[[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] isEqualToString:@"Screen"]);

    webView.get().mediaType = @"print";
    EXPECT_TRUE([[[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] isEqualToString:@"Print"]);

    webView.get().mediaType = @"lavalamp";
    EXPECT_TRUE([[[webView stringByEvaluatingJavaScript:@"document.body.innerText"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] isEqualToString:@"LavaLamp"]);
}
