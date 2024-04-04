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
    RetainPtr handler = adoptNS([[StoreBlobMessageHandler alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"StoreBlobToBeDeleted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request.get()];

    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Success", (NSString *)[lastScriptMessage body]);

    String hash = WebCore::SQLiteFileSystem::computeHashForFileName("StoreBlobToBeDeleted"_s);
    RetainPtr originURL = [NSURL URLWithString:@"file://"];
    __block RetainPtr<NSString> originDirectoryString;
    readyToContinue = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL.get() topOrigin:originURL.get() type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        originDirectoryString = result;
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    RetainPtr originDirectory = [NSURL fileURLWithPath:originDirectoryString.get() isDirectory:YES];
    RetainPtr databaseDirectory = [originDirectory URLByAppendingPathComponent:hash];
    RetainPtr blobFileURL = [databaseDirectory URLByAppendingPathComponent:@"1.blob"];
    RetainPtr databaseFileURL = [databaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];

    // The database file and blob file should definitely be there right now.
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.get().path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.get().path]);

    // To make sure that the -wal and -shm files for a database are deleted even if the sqlite3 file is already missing,
    // we need to:
    // 1 - Create the path for a fake database that won't actively be in use
    // 2 - Move -wal and -shm files into that directory
    // 3 - Make sure the entire directory is deleted
    String fakeHash = WebCore::SQLiteFileSystem::computeHashForFileName("FakeDatabasePath"_s);
    RetainPtr fakeDatabaseDirectory = [originDirectory URLByAppendingPathComponent:fakeHash];
    RetainPtr fakeShmURL = [fakeDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"];
    RetainPtr fakeWalURL = [fakeDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3-shm"];
    [[NSFileManager defaultManager] createDirectoryAtURL:fakeDatabaseDirectory.get() withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL.get() toURL:fakeShmURL.get() error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:databaseFileURL.get() toURL:fakeWalURL.get() error:nil];

    // Make some other .blob files in the database directory to later validate all blob files in the directory are deleted.
    RetainPtr otherBlob1 = [databaseDirectory URLByAppendingPathComponent:@"7182.blob"];
    RetainPtr otherBlob2 = [databaseDirectory URLByAppendingPathComponent:@"1a.blob"];
    RetainPtr otherBlob3 = [databaseDirectory URLByAppendingPathComponent:@"a1.blob"];
    RetainPtr otherBlob4 = [databaseDirectory URLByAppendingPathComponent:@".blob"];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL.get() toURL:otherBlob1.get() error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL.get() toURL:otherBlob2.get() error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL.get() toURL:otherBlob3.get() error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:blobFileURL.get() toURL:otherBlob4.get() error:nil];

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.get().path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.get().path]);

        // Make sure all fake blob file are gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob1.get().path]);

        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob2.get().path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob3.get().path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob4.get().path]);

        // Make sure everything related to the fake database is gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeShmURL.get().path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeWalURL.get().path]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeDatabaseDirectory.get().path]);

        // Now delete them so we're not leaving files around.
        [[NSFileManager defaultManager] removeItemAtURL:otherBlob2.get() error:nil];
        [[NSFileManager defaultManager] removeItemAtURL:otherBlob3.get() error:nil];
        [[NSFileManager defaultManager] removeItemAtURL:otherBlob4.get() error:nil];

        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(IndexedDB, StoreBlobThenDeleteDatabase)
{
    RetainPtr handler = adoptNS([[StoreBlobMessageHandler alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"StoreBlobToBeDeleted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request.get()];
    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Success", (NSString *)[lastScriptMessage body]);

    String hash = WebCore::SQLiteFileSystem::computeHashForFileName("StoreBlobToBeDeleted"_s);
    RetainPtr originURL = [NSURL URLWithString:@"file://"];
    __block RetainPtr<NSString> originDirectoryString;
    readyToContinue = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL.get() topOrigin:originURL.get() type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        originDirectoryString = result;
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    RetainPtr originDirectory = [NSURL fileURLWithPath:originDirectoryString.get() isDirectory:YES];
    RetainPtr databaseDirectory = [originDirectory URLByAppendingPathComponent:hash];
    RetainPtr blobFileURL = [databaseDirectory URLByAppendingPathComponent:@"1.blob"];
    RetainPtr databaseFileURL = [databaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.get().path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.get().path]);

    // Add a .blob file that is not created by IndexedDB API.
    RetainPtr anotherBlobFileURL = [databaseDirectory URLByAppendingPathComponent:@"7182.blob"];
    [[NSFileManager defaultManager] copyItemAtPath:blobFileURL.get().path toPath:anotherBlobFileURL.get().path error:nil];

    readyToContinue = false;
    [webView evaluateJavaScript:@"deleteDatabase(() => { sendMessage('Delete success'); })" completionHandler:nil];
    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Delete success", (NSString *)[lastScriptMessage body]);

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:blobFileURL.get().path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:databaseFileURL.get().path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:anotherBlobFileURL.get().path]);
}
