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
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/Deque.h>
#import <wtf/RetainPtr.h>

static bool receivedScriptMessage;
static Deque<RetainPtr<WKScriptMessage>> scriptMessages;

@interface IndexedDBMPMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBMPMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessages.append(message);
}

@end

static WKScriptMessage *getNextMessage()
{
    if (scriptMessages.isEmpty()) {
        receivedScriptMessage = false;
        TestWebKitAPI::Util::run(&receivedScriptMessage);
    }

    return [[scriptMessages.takeFirst() retain] autorelease];
}

TEST(IndexedDB, IndexedDBMultiProcess)
{
    RetainPtr<IndexedDBMPMessageHandler> handler = adoptNS([[IndexedDBMPMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    [configuration.get().processPool _terminateNetworkProcess];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBMultiProcess-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    RetainPtr<NSString> string1 = (NSString *)[getNextMessage() body];
    RetainPtr<NSString> string2 = (NSString *)[getNextMessage() body];
    RetainPtr<NSString> string3 = (NSString *)[getNextMessage() body];

    // Make a new web view with a new web process to continue the test
    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBMultiProcess-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView2 loadRequest:request];

    RetainPtr<NSString> string4 = (NSString *)[getNextMessage() body];

    EXPECT_WK_STREQ(@"UpgradeNeeded", string1.get());
    EXPECT_WK_STREQ(@"Transaction complete", string2.get());
    EXPECT_WK_STREQ(@"Open success", string3.get());
    EXPECT_WK_STREQ(@"Value of foo: bar", string4.get());

    RetainPtr<NSString> string5 = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Get loops started", string5.get());

    // Make a new web view with a new web process to continue the test
    RetainPtr<WKWebView> webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBMultiProcess-3" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView3 loadRequest:request];

    RetainPtr<NSString> string6 = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Blocked!", string6.get());

    // Kill the blocking web process
    [webView2 _killWebContentProcessAndResetState];

    RetainPtr<NSString> string7 = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Deleted!", string7.get());
}
