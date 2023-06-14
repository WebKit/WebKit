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
#import <WebCore/SQLiteFileSystem.h>
#import <WebKit/WebKit.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

@interface StoreBlobMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation StoreBlobMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    readyToContinue = true;
    lastScriptMessage = message;
}

@end

TEST(IndexedDB, StoreBlobThenRemoveData)
{
    auto handler = adoptNS([[StoreBlobMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"StoreBlobToBeDeleted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Success", (NSString *)[lastScriptMessage body]);

    NSString *hash = WebCore::SQLiteFileSystem::computeHashForFileName("StoreBlobToBeDeleted"_s);
    NSURL *originURL = [NSURL URLWithString:@"file://"];
    __block NSString *originDirectoryString = nil;
    readyToContinue = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL topOrigin:originURL type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        originDirectoryString = result;
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    NSURL *originDirectory = [NSURL fileURLWithPath:originDirectoryString isDirectory:YES];
    NSURL *databaseDirectory = [originDirectory URLByAppendingPathComponent:hash];
    NSURL *blobFileURL = [databaseDirectory URLByAppendingPathComponent:@"1.blob"];
    NSURL *databaseFileURL = [databaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];

    // The database file and blob file should definitely be there right now.
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.path]);

    // To make sure that the -wal and -shm files for a database are deleted even if the sqlite3 file is already missing,
    // we need to:
    // 1 - Create the path for a fake database that won't actively be in use
    // 2 - Move -wal and -shm files into that directory
    // 3 - Make sure the entire directory is deleted
    NSString *fakeHash = WebCore::SQLiteFileSystem::computeHashForFileName("FakeDatabasePath"_s);
    NSURL *fakeDatabaseDirectory = [originDirectory URLByAppendingPathComponent:fakeHash];
    NSURL *fakeShmURL = [fakeDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"];
    NSURL *fakeWalURL = [fakeDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3-shm"];
    [[NSFileManager defaultManager] createDirectoryAtURL:fakeDatabaseDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL toURL:fakeShmURL error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:databaseFileURL toURL:fakeWalURL error:nil];

    // Make some other .blob files in the database directory to later validate all blob files in the directory are deleted.
    NSURL *otherBlob1 = [databaseDirectory URLByAppendingPathComponent:@"7182.blob"];
    NSURL *otherBlob2 = [databaseDirectory URLByAppendingPathComponent:@"1a.blob"];
    NSURL *otherBlob3 = [databaseDirectory URLByAppendingPathComponent:@"a1.blob"];
    NSURL *otherBlob4 = [databaseDirectory URLByAppendingPathComponent:@".blob"];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL toURL:otherBlob1 error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL toURL:otherBlob2 error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL toURL:otherBlob3 error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL toURL:otherBlob4 error:nil];

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.path]);

        // Make sure all fake blob file are gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob1.path]);

        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob2.path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob3.path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob4.path]);

        // Make sure everything related to the fake database is gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeShmURL.path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeWalURL.path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeDatabaseDirectory.path]);

        // Now delete them so we're not leaving files around.
        [[NSFileManager defaultManager] removeItemAtURL:otherBlob2 error:nil];
        [[NSFileManager defaultManager] removeItemAtURL:otherBlob3 error:nil];
        [[NSFileManager defaultManager] removeItemAtURL:otherBlob4 error:nil];

        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(IndexedDB, StoreBlobThenDeleteDatabase)
{
    auto handler = adoptNS([[StoreBlobMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"StoreBlobToBeDeleted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Success", (NSString *)[lastScriptMessage body]);

    NSString *hash = WebCore::SQLiteFileSystem::computeHashForFileName("StoreBlobToBeDeleted"_s);
    NSURL *originURL = [NSURL URLWithString:@"file://"];
    __block NSString *originDirectoryString = nil;
    readyToContinue = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL topOrigin:originURL type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        originDirectoryString = result;
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    NSURL *originDirectory = [NSURL fileURLWithPath:originDirectoryString isDirectory:YES];
    NSURL *databaseDirectory = [originDirectory URLByAppendingPathComponent:hash];
    NSURL *blobFileURL = [databaseDirectory URLByAppendingPathComponent:@"1.blob"];
    NSURL *databaseFileURL = [databaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.path]);

    // Add a .blob file that is not created by IndexedDB API.
    NSURL *anotherBlobFileURL = [databaseDirectory URLByAppendingPathComponent:@"7182.blob"];
    [[NSFileManager defaultManager] copyItemAtPath:blobFileURL.path toPath:anotherBlobFileURL.path error:nil];

    readyToContinue = false;
    [webView evaluateJavaScript:@"deleteDatabase(() => { sendMessage('Delete success'); })" completionHandler:nil];
    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Delete success", (NSString *)[lastScriptMessage body]);

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:anotherBlobFileURL.path]);
}
