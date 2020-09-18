/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Deque.h>
#import <wtf/RetainPtr.h>

// FIXME: Re-enable this test once rdar://57029120 is resolved.
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 110000) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < 140000)

static bool receivedScriptMessage;
static Deque<RetainPtr<WKScriptMessage>> scriptMessages;

@interface IndexedDBStructuredCloneBackwardCompatibilityMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBStructuredCloneBackwardCompatibilityMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessages.append(message);
}

@end

@interface StructuredCloneBackwardCompatibilityNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation StructuredCloneBackwardCompatibilityNavigationDelegate

- (NSData *)_webCryptoMasterKeyForWebView:(WKWebView *)webView
{
    return [NSData dataWithBytes:(const uint8_t*)"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f" length:16];
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

TEST(IndexedDB, StructuredCloneBackwardCompatibility)
{
    RetainPtr<IndexedDBStructuredCloneBackwardCompatibilityMessageHandler> handler
        = adoptNS([[IndexedDBStructuredCloneBackwardCompatibilityMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    // Copy the baked database files to the database directory
    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"IndexedDBStructuredCloneBackwardCompatibility" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url2 = [[NSBundle mainBundle] URLForResource:@"IndexedDBStructuredCloneBackwardCompatibility" withExtension:@"sqlite3-shm" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *url3 = [[NSBundle mainBundle] URLForResource:@"IndexedDBStructuredCloneBackwardCompatibility" withExtension:@"sqlite3-wal" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *idbPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] removeItemAtURL:idbPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);

    RetainPtr<_WKWebsiteDataStoreConfiguration> websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = idbPath;
    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];

    idbPath = [idbPath URLByAppendingPathComponent:@"file__0"];
    idbPath = [idbPath URLByAppendingPathComponent:@"backward_compatibility"];
    [[NSFileManager defaultManager] createDirectoryAtURL:idbPath withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:[idbPath URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url2 toURL:[idbPath URLByAppendingPathComponent:@"IndexedDB.sqlite3-shm"] error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url3 toURL:[idbPath URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"] error:nil];

    // Run the test
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[StructuredCloneBackwardCompatibilityNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBStructuredCloneBackwardCompatibilityRead" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    EXPECT_STREQ([getNextMessage().body UTF8String], "Pass");
}
#endif
