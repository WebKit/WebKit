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
#import <WebKit/WKWebsiteDataStore.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool readyToContinue;

@interface LocalStorageClearMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation LocalStorageClearMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    readyToContinue = true;
}

@end

TEST(WKWebView, LocalStorageClear)
{
    RetainPtr<LocalStorageClearMessageHandler> handler = adoptNS([[LocalStorageClearMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    [configuration _setAllowUniversalAccessFromFileURLs:YES];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"LocalStorageClear" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    webView = nil;

    NSString *dbPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/file__0.localstorage" stringByExpandingTildeInPath];
    NSString *dbSHMPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/file__0.localstorage-shm" stringByExpandingTildeInPath];
    NSString *dbWALPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/file__0.localstorage-wal" stringByExpandingTildeInPath];
    NSString *trackerPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/StorageTracker.db" stringByExpandingTildeInPath];
    NSString *trackerSHMPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/StorageTracker.db-shm" stringByExpandingTildeInPath];
    NSString *trackerWALPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/StorageTracker.db-wal" stringByExpandingTildeInPath];

    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:dbPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:dbSHMPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:dbWALPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:trackerPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:trackerSHMPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:trackerWALPath]);
        readyToContinue = true;
    }];

    TestWebKitAPI::Util::run(&readyToContinue);
}

#endif
