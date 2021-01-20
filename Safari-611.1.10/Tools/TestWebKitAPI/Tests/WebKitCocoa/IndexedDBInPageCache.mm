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

#import "config.h"

#import "PlatformUtilities.h"
#import "Test.h"

#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

static bool receivedScriptMessage;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface IndexedDBInPageCacheMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBInPageCacheMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

TEST(IndexedDB, IndexedDBInPageCache)
{
    auto handler = adoptNS([[IndexedDBInPageCacheMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load page that holds open database connection.
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBInPageCache" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string1 = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"First Database Connection Opened", string1.get());

    // Load another page that deletes database.
    NSURLRequest *request2 = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBNotInPageCache" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request2];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Database Deleted", string2.get());

    [webView goBack];
    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string3 = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"First Database Connection Closed, Second Database Connection Not Failed, Third Database Connection Opened", string3.get());
}
