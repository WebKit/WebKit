/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

TEST(WKWebView, DISABLED_SetOverrideContentSecurityPolicyWithEmptyStringForPageWithCSP)
{
    @autoreleasepool {
        RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration _setOverrideContentSecurityPolicy:@""];

        RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"page-with-csp" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        [webView waitForMessage:@"MainFrame: A"];
        [webView waitForMessage:@"MainFrame: B"];
        [webView waitForMessage:@"Subframe: A"];
        [webView waitForMessage:@"Subframe: B"];
    }
}

TEST(WKWebView, SetOverrideContentSecurityPolicyForPageWithCSP)
{
    @autoreleasepool {
        RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration _setOverrideContentSecurityPolicy:@"script-src 'nonce-b'"];

        RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"page-with-csp" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        [webView waitForMessage:@"MainFrame: B"];
        [webView waitForMessage:@"Subframe: B"];
    }
}

TEST(WKWebView, SetOverrideContentSecurityPolicyForPageWithoutCSP)
{
    @autoreleasepool {
        RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration _setOverrideContentSecurityPolicy:@"script-src 'nonce-b'"];

        RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"page-without-csp" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        [webView waitForMessage:@"MainFrame: B"];
        [webView waitForMessage:@"Subframe: B"];
    }
}

TEST(WKWebView, CheckViolationReportDocumentURIForFileProtocol)
{
    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"csp-document-uri-report" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        [webView waitForMessage:@"document-uri: file"];
    }
}

TEST(WKWebView, CheckViolationReportDocumentURIForDataProtocol)
{
    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSString *path = [NSBundle.mainBundle pathForResource:@"csp-document-uri-report" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"];
        NSString* content = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL];

        NSURLRequest *loadRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html"]];
        NSData *data = [content dataUsingEncoding:NSUTF8StringEncoding];
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@"data:text/html"] MIMEType:@"text/HTML" expectedContentLength:[data length] textEncodingName:@"UTF-8"]);

        [webView loadSimulatedRequest:loadRequest response:response.get() responseData:data];
        [webView waitForMessage:@"document-uri: data"];
    }
}

TEST(WKWebView, CheckViolationReportDocumentURIForAboutProtocol)
{
    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSString *path = [NSBundle.mainBundle pathForResource:@"csp-document-uri-report" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"];
        NSString* content = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL];

        [webView loadHTMLString:content baseURL:nil];
        [webView waitForMessage:@"document-uri: about"];
    }
}
