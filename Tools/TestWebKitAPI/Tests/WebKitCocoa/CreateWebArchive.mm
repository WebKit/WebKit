
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace TestWebKitAPI {

static const char* mainBytes = R"TESTRESOURCE(
<head>
<script src="webarchivetest://host/script.js"></script>
</head>
)TESTRESOURCE";

static const char* scriptBytes = R"TESTRESOURCE(
window.webkit.messageHandlers.testHandler.postMessage("done");
)TESTRESOURCE";

TEST(WebArchive, CreateCustomScheme)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        const char* bytes = nullptr;
        NSString *mimeType;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            bytes = mainBytes;
            mimeType = @"text/html";
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/script.js"]) {
            bytes = scriptBytes;
            mimeType = @"application/javascript";
        } else
            FAIL();

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:bytes length:strlen(bytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    static bool done = false;
    [webView performAfterReceivingMessage:@"done" action:[&] { done = true; }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host/main.html"]]];

    Util::run(&done);
    done = false;

    static RetainPtr<NSData> archiveData;
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE(!!result);
        archiveData = result;
        done = true;
    }];

    Util::run(&done);
    done = false;

    [webView performAfterReceivingMessage:@"done" action:[&] { done = true; }];
    [webView loadData:archiveData.get() MIMEType:(NSString *)kUTTypeArchive characterEncodingName:@"utf-8" baseURL:[NSURL URLWithString:@"about:blank"]];

    Util::run(&done);
    done = false;
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
