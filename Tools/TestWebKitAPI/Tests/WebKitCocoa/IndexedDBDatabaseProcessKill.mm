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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

static bool receivedAtLeastOneOpenError;
static bool receivedAtLeastOneDeleteError;
static bool openRequestUpgradeNeeded;
static bool databaseErrorReceived;

@interface DatabaseProcessKillMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation DatabaseProcessKillMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;

    if ([[message body] isEqualToString:@"DatabaseErrorReceived"]) {
        databaseErrorReceived = true;
        return;
    }

    if ([[message body] isEqualToString:@"OpenRequestUpgradeNeeded"]) {
        openRequestUpgradeNeeded = true;
        return;
    }

    if ([[message body] isEqualToString:@"DeleteRequestError"]) {
        receivedAtLeastOneDeleteError = true;
        return;
    }

    if ([[message body] isEqualToString:@"OpenRequestError"]) {
        receivedAtLeastOneOpenError = true;
        return;
    }

    lastScriptMessage = message;
}

@end

TEST(IndexedDB, DatabaseProcessKill)
{
    auto handler = adoptNS([[DatabaseProcessKillMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBDatabaseProcessKill-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    bool killedDBProcess = false;
    while (true) {
        if (databaseErrorReceived)
            break;

        TestWebKitAPI::Util::run(&receivedScriptMessage);
        receivedScriptMessage = false;
        if (!killedDBProcess && openRequestUpgradeNeeded) {
            killedDBProcess = true;
            [configuration.get().websiteDataStore _terminateNetworkProcess];
        }
    }

    EXPECT_EQ(receivedAtLeastOneOpenError, true);
    EXPECT_EQ(receivedAtLeastOneDeleteError, true);
    EXPECT_EQ(databaseErrorReceived, true);
}

TEST(IndexedDB, OneVMPerThread)
{
    RetainPtr<DatabaseProcessKillMessageHandler> handler = adoptNS([[DatabaseProcessKillMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto secondWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    
    NSString *htmlString = @"<script> \
        function openDatabase() { \
            var request = indexedDB.open('testDB'); \
            request.onupgradeneeded = function(event) { \
                let db = event.target.result; \
                let os = db.createObjectStore('testOS');\
                for (let i = 0; i < 10000; i++) \
                    os.put(i, i); \
                webkit.messageHandlers.testHandler.postMessage('Opened');\
            }; \
        }\
        </script>";

    [webView synchronouslyLoadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [secondWebView synchronouslyLoadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://apple.com"]];

    receivedScriptMessage = false;
    [webView evaluateJavaScript:@"openDatabase()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Opened", [lastScriptMessage body]);

    kill([webView _webProcessIdentifier], SIGKILL);

    receivedScriptMessage = false;
    [secondWebView evaluateJavaScript:@"openDatabase()" completionHandler:nil];
    lastScriptMessage = nil;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Opened", [lastScriptMessage body]);
}
