/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "ContentSecurityPolicyTestHelpers.h"

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebCore/Color.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

static HTTPServer pdfServerWithSandboxCSPDirective()
{
    RetainPtr pdfURL = [NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"];
    HTTPResponse response { [NSData dataWithContentsOfURL:pdfURL.get()] };
    response.headerFields.set("Content-Security-Policy"_s, "sandbox allow-scripts;"_s);
    return { { { "/"_s, response } } };
}

void runLoadPDFWithSandboxCSPDirectiveTest(TestWKWebView *webView)
{
    HTTPServer server { pdfServerWithSandboxCSPDirective() };

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];
    auto colorsWithPlainResponse = [webView sampleColors];

    [webView synchronouslyLoadRequest:server.request()];
    auto colorsWithCSPResponse = [webView sampleColors];

    EXPECT_EQ(colorsWithPlainResponse, colorsWithCSPResponse);
}

}
