/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "Test.h"
#import <WebKit/WebKit.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool receivedScriptMessage;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface IndexedDBMPMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBMPMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

TEST(IndexedDB, IndexedDBMultiProcess)
{
    RetainPtr<IndexedDBMPMessageHandler> handler = adoptNS([[IndexedDBMPMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration.get().processPool _terminateDatabaseProcess];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBMultiProcess-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string1 = (NSString *)[lastScriptMessage body];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string3 = (NSString *)[lastScriptMessage body];

    // Make a new web view with a new web process to finish the test
    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBMultiProcess-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView2 loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string4 = (NSString *)[lastScriptMessage body];

    EXPECT_WK_STREQ(@"UpgradeNeeded", string1.get());
    EXPECT_WK_STREQ(@"Transaction complete", string2.get());
    EXPECT_WK_STREQ(@"Open success", string3.get());
    EXPECT_WK_STREQ(@"Value of foo: bar", string4.get());
}

#endif
