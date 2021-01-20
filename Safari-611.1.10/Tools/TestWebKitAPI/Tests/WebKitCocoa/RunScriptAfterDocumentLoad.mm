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

#import "config.h"

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

static const char* markup =
    "<head>"
    "<script>function scriptLoaded(element) { webkit.messageHandlers.testHandler.postMessage(`${element.getAttribute('src')} loaded`) }</script>"
    "<script onload='scriptLoaded(this)' src='async.js' async></script>"
    "</head>"
    "<body>"
    "<script>document.addEventListener('DOMContentLoaded', () => webkit.messageHandlers.testHandler.postMessage('DOMContentLoaded'))</script>"
    "<script onload='scriptLoaded(this)' src='defer.js' defer></script>"
    "<script onload='scriptLoaded(this)' src='sync.js'></script>"
    "</body>";

TEST(RunScriptAfterDocumentLoad, ExecutionOrderOfScriptsInDocument)
{
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"custom"];
    [configuration _setShouldDeferAsynchronousScriptsUntilAfterDocumentLoad:YES];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto responseBlock = [task = retainPtr(task)] {
            NSURL *requestURL = [task request].URL;
            NSString *script = [NSString stringWithFormat:@"webkit.messageHandlers.testHandler.postMessage('Running %@')", requestURL.absoluteString.lastPathComponent];
            auto response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/javascript" expectedContentLength:[script length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:[script dataUsingEncoding:NSUTF8StringEncoding]];
            [task didFinish];
        };

        if ([[task request].URL.absoluteString containsString:@"async.js"]) {
            responseBlock();
            return;
        }

        // Delay request responses for the other two scripts for a short duration to ensure that in the absence
        // of the deferred asynchronous scripts configuration flag, we will normally execute the asynchronous script
        // before the other scripts.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (0.25_s).nanoseconds()), dispatch_get_main_queue(), responseBlock);
    }];

    auto messages = adoptNS([NSMutableArray new]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200) configuration:configuration.get()]);
    for (NSString *message in @[ @"Running async.js", @"Running defer.js", @"Running sync.js", @"DOMContentLoaded", @"async.js loaded", @"defer.js loaded", @"sync.js loaded" ]) {
        [webView performAfterReceivingMessage:message action:[message = adoptNS(message.copy), messages] {
            [messages addObject:message.get()];
        }];
    }

    [webView loadHTMLString:@(markup) baseURL:[NSURL URLWithString:@"custom://"]];
    [webView _test_waitForDidFinishNavigation];

    // Verify that the asynchronous script is executed after document load, the synchronous script is executed before the
    // deferred script, and the deferred script is executed prior to document load.
    EXPECT_EQ(7U, [messages count]);
    EXPECT_WK_STREQ("Running sync.js", [messages objectAtIndex:0]);
    EXPECT_WK_STREQ("sync.js loaded", [messages objectAtIndex:1]);
    EXPECT_WK_STREQ("Running defer.js", [messages objectAtIndex:2]);
    EXPECT_WK_STREQ("defer.js loaded", [messages objectAtIndex:3]);
    EXPECT_WK_STREQ("DOMContentLoaded", [messages objectAtIndex:4]);
    EXPECT_WK_STREQ("Running async.js", [messages objectAtIndex:5]);
    EXPECT_WK_STREQ("async.js loaded", [messages objectAtIndex:6]);
}
