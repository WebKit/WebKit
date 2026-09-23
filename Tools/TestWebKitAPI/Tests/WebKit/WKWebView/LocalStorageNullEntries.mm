/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

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
    NSURL *url1 = [NSBundle.test_resourcesBundle URLForResource:@"LocalStorageNullEntries" withExtension:@"localstorage"];
    NSURL *url2 = [NSBundle.test_resourcesBundle URLForResource:@"LocalStorageNullEntries" withExtension:@"localstorage-shm"];

    NSURL *targetURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/LocalStorage/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] createDirectoryAtURL:targetURL withIntermediateDirectories:YES attributes:nil error:nil];

    NSURL *localStorageURL = [targetURL URLByAppendingPathComponent:@"file__0.localstorage"];
    NSURL *localStorageSHMURL = [targetURL URLByAppendingPathComponent:@"file__0.localstorage-shm"];

    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:localStorageURL error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url2 toURL:localStorageSHMURL error:nil];

    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"LocalStorageNullEntries" withExtension:@"html"]];
        [webView loadRequest:request];

        readyToContinue = false;
        TestWebKitAPI::Util::run(&readyToContinue);

        webView = nil;
    }

    // Clean up using WKWebsiteDataStore to properly close the database.
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:localStorageURL.path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:localStorageSHMURL.path]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}
