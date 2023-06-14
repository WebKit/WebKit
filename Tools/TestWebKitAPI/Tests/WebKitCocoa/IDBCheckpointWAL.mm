/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

@interface IDBCheckpointWALMessageHandler : NSObject <WKScriptMessageHandler>
@property (nonatomic, assign) bool receivedScriptMessage;
@property (nonatomic, retain) WKScriptMessage *lastScriptMessage;
@end

@implementation IDBCheckpointWALMessageHandler

@synthesize receivedScriptMessage = _receivedScriptMessage;
@synthesize lastScriptMessage = _lastScriptMessage;

- (void)dealloc
{
    [_lastScriptMessage release];
    [super dealloc];
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    _receivedScriptMessage = YES;
    self.lastScriptMessage = message;
}

- (bool *)receivedScriptMessagePointer
{
    return &_receivedScriptMessage;
}

@end

long long fileSizeAtPath(NSString *path)
{
    NSDictionary *attrs = [[NSFileManager defaultManager] fileAttributesAtPath:path traverseLink:YES];
    return [attrs[NSFileSize] longLongValue];
}

TEST(IndexedDB, CheckpointsWALAutomatically)
{
    auto handler = adoptNS([[IDBCheckpointWALMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration.get().websiteDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [configuration.get().websiteDataStore _terminateNetworkProcess];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IDBCheckpointWAL" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run([handler receivedScriptMessagePointer]);
    EXPECT_STREQ([[[handler lastScriptMessage] body] UTF8String], "Success");

    NSURL *originURL = [NSURL URLWithString:@"file://"];
    __block NSString *indexedDBDirectoryString = nil;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL topOrigin:originURL type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        indexedDBDirectoryString = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    NSURL *indexedDBDirectory = [NSURL fileURLWithPath:indexedDBDirectoryString isDirectory:YES];
    NSString *databaseHash = WebCore::SQLiteFileSystem::computeHashForFileName("test-wal-checkpoint"_s);
    NSURL *indexedDBDatabaseDirectory = [indexedDBDirectory URLByAppendingPathComponent:databaseHash];
    NSURL *indexedDBDatabaseFile = [indexedDBDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];
    NSURL *indexedDBDatabaseWAL = [indexedDBDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"];

    // We inserted a single 5 MB row, which is greater than the WAL auto-checkpoint
    // threshold of 4MB, so the WAL should be of minimal size now.
    EXPECT_LT(fileSizeAtPath(indexedDBDatabaseWAL.path), 128 * 1024);

    // Since the single 5 MB row was checkpointed to the main DB file, it should be at least that large.
    EXPECT_GT(fileSizeAtPath(indexedDBDatabaseFile.path), 5 * 1024 * 1024);
}
