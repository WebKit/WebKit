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
#import "Test.h"
#import <WebCore/SQLiteFileSystem.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

static bool readyToContinue;
static bool receivedScriptMessage;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface IndexedDBFileSizeMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBFileSizeMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

TEST(IndexedDB, IndexedDBTempFileSize)
{
    auto handler = adoptNS([[IndexedDBFileSizeMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    NSString *hash = WebCore::SQLiteFileSystem::computeHashForFileName("IndexedDBTempFileSize");
    NSString *databaseRootDirectory = [@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath];
    NSString *databaseDirectory = [[[databaseRootDirectory stringByAppendingPathComponent:@"v1"] stringByAppendingPathComponent:@"file__0"] stringByAppendingPathComponent:hash];
    RetainPtr<NSURL> idbPath = [NSURL fileURLWithPath:databaseRootDirectory isDirectory:YES];
    RetainPtr<NSURL> walFilePath = [NSURL fileURLWithPath:[databaseDirectory stringByAppendingPathComponent:@"IndexedDB.sqlite3-wal"] isDirectory:NO];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = idbPath.get();

    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];
    auto types = adoptNS([[NSSet alloc] initWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil]);

    [configuration.get().websiteDataStore removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    readyToContinue = false;
    TestWebKitAPI::Util::run(&readyToContinue);

    // Do some IndexedDB operations to generate WAL file.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBTempFileSize-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string1 = (NSString *)[lastScriptMessage body];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];

    // Terminate network process to keep WAL on disk.
    webView = nil;
    [configuration.get().processPool _terminateNetworkProcess];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:walFilePath.get().path]);
    RetainPtr<NSDictionary> fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:walFilePath.get().path error:nil];
    NSNumber *fileSizeBefore = [fileAttributes objectForKey:NSFileSize];

    // Open the same database again.
    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBTempFileSize-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string3 = (NSString *)[lastScriptMessage body];

    fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:walFilePath.get().path error:nil];
    NSNumber *fileSizeAfter = [fileAttributes objectForKey:NSFileSize];
    EXPECT_GT([fileSizeBefore longLongValue], [fileSizeAfter longLongValue]);

    EXPECT_WK_STREQ(@"UpgradeNeeded", string1.get());
    EXPECT_WK_STREQ(@"Success", string2.get());
    EXPECT_WK_STREQ(@"Success", string3.get()); 
}
