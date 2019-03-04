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
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

static bool readyToContinue;

@interface LocalStorageNullEntriesMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation LocalStorageNullEntriesMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    readyToContinue = true;
}

@end

TEST(WKWebView, LocalStorageNullEntries)
{
    RetainPtr<LocalStorageNullEntriesMessageHandler> handler = adoptNS([[LocalStorageNullEntriesMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    [configuration _setNeedsStorageAccessFromFileURLsQuirk:NO];
    [configuration _setAllowUniversalAccessFromFileURLs:YES];

    // Copy the inconsistent database files to the LocalStorage directory
    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"LocalStorageNullEntries" withExtension:@"localstorage" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"LocalStorageNullEntries" withExtension:@"localstorage-shm" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *targetURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] createDirectoryAtURL:targetURL withIntermediateDirectories:YES attributes:nil error:nil];

    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:[targetURL URLByAppendingPathComponent:@"file__0.localstorage"] error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url2 toURL:[targetURL URLByAppendingPathComponent:@"file__0.localstorage-shm"] error:nil];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"LocalStorageNullEntries" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    readyToContinue = false;
    TestWebKitAPI::Util::run(&readyToContinue);

    webView = nil;

    // Clean up.
    [[NSFileManager defaultManager] removeItemAtURL:[targetURL URLByAppendingPathComponent:@"file__0.localstorage"] error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:[targetURL URLByAppendingPathComponent:@"file__0.localstorage-shm"] error:nil];
}
