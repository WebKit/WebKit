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

#import "DeprecatedGlobalValues.h"
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

@interface IndexedDBFileSizeMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBFileSizeMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
    [receivedMessages addObject:message.body];
}

@end

TEST(IndexedDB, IndexedDBTempFileSize)
{
    RetainPtr handler = adoptNS([[IndexedDBFileSizeMessageHandler alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr originURL = [NSURL URLWithString:@"file://"];
    __block RetainPtr<NSString> databaseRootDirectoryString;
    readyToContinue = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL.get() topOrigin:originURL.get() type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        databaseRootDirectoryString = result;
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    RetainPtr databaseRootDirectory = [NSURL fileURLWithPath:databaseRootDirectoryString.get() isDirectory:YES];
    String hash = WebCore::SQLiteFileSystem::computeHashForFileName("IndexedDBTempFileSize"_s);
    RetainPtr databaseDirectory = [databaseRootDirectory URLByAppendingPathComponent:hash];
    RetainPtr walFilePath = [databaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"];

    RetainPtr types = adoptNS([[NSSet alloc] initWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil]);
    [configuration.get().websiteDataStore removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    readyToContinue = false;
    TestWebKitAPI::Util::run(&readyToContinue);

    // Do some IndexedDB operations to generate WAL file.
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBTempFileSize-1" withExtension:@"html"]];
    [webView loadRequest:request.get()];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        return [[lastScriptMessage body] isEqualToString:@"Success"];
    }, 10, @"Warning: expected 'Success' message after initializing database");

    // Terminate network process to keep WAL on disk.
    webView = nil;
    [configuration.get().websiteDataStore _terminateNetworkProcess];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:walFilePath.get().path]);
    RetainPtr fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:walFilePath.get().path error:nil];
    NSNumber *fileSizeBefore = [fileAttributes objectForKey:NSFileSize];

    // Open the same database again.
    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBTempFileSize-2" withExtension:@"html"]];

    receivedScriptMessage = false;
    [webView loadRequest:request.get()];
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:walFilePath.get().path error:nil];
    RetainPtr fileSizeAfter = [fileAttributes objectForKey:NSFileSize];
    EXPECT_GT([fileSizeBefore longLongValue], [fileSizeAfter longLongValue]);

    EXPECT_EQ([receivedMessages count], 3U);
    EXPECT_WK_STREQ(@"UpgradeNeeded", [receivedMessages objectAtIndex:0]);
    EXPECT_WK_STREQ(@"Success", [receivedMessages objectAtIndex:1]);
    EXPECT_WK_STREQ(@"Success", [receivedMessages objectAtIndex:2]);
}
