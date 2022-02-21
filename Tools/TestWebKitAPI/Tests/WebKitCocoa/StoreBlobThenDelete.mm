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
    RetainPtr<StoreBlobMessageHandler> handler = adoptNS([[StoreBlobMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"StoreBlobToBeDeleted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Success", (NSString *)[lastScriptMessage body]);

    NSString *hash = WebCore::SQLiteFileSystem::computeHashForFileName("StoreBlobToBeDeleted");
    NSString *originDirectory = @"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/IndexedDB/v1/file__0/";
    NSString *databaseDirectory = [[originDirectory stringByAppendingString:hash] stringByExpandingTildeInPath];
    NSString *blobFilePath = [databaseDirectory stringByAppendingPathComponent:@"1.blob"];
    NSString *databaseFilePath = [databaseDirectory stringByAppendingPathComponent:@"IndexedDB.sqlite3"];

    // The database file and blob file should definitely be there right now.
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:blobFilePath]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:databaseFilePath]);

    // To make sure that the -wal and -shm files for a database are deleted even if the sqlite3 file is already missing,
    // we need to:
    // 1 - Create the path for a fake database that won't actively be in use
    // 2 - Move -wal and -shm files into that directory
    // 3 - Make sure the entire directory is deleted
    NSString *fakeHash = WebCore::SQLiteFileSystem::computeHashForFileName("FakeDatabasePath");
    NSString *fakeDatabaseDirectory = [[originDirectory stringByAppendingString:fakeHash] stringByExpandingTildeInPath];
    NSString *fakeShmPath = [fakeDatabaseDirectory stringByAppendingPathComponent:@"IndexedDB.sqlite3-wal"];
    NSString *fakeWalPath = [fakeDatabaseDirectory stringByAppendingPathComponent:@"IndexedDB.sqlite3-shm"];
    [[NSFileManager defaultManager] createDirectoryAtPath:fakeDatabaseDirectory withIntermediateDirectories:NO attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:databaseFilePath toPath:fakeShmPath error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:databaseFilePath toPath:fakeWalPath error:nil];

    // Make some other .blob files in the database directory to later validate all blob files in the directory are deleted.
    NSString *otherBlob1 = [databaseDirectory stringByAppendingPathComponent:@"7182.blob"];
    NSString *otherBlob2 = [databaseDirectory stringByAppendingPathComponent:@"1a.blob"];
    NSString *otherBlob3 = [databaseDirectory stringByAppendingPathComponent:@"a1.blob"];
    NSString *otherBlob4 = [databaseDirectory stringByAppendingPathComponent:@".blob"];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob1 error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob2 error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob3 error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob4 error:nil];

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:blobFilePath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:databaseFilePath]);

        // Make sure all fake blob file are gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob1]);

        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob2]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob3]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob4]);

        // Make sure everything related to the fake database is gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeShmPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeWalPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeDatabaseDirectory]);

        // Now delete them so we're not leaving files around.
        [[NSFileManager defaultManager] removeItemAtPath:otherBlob2 error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:otherBlob3 error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:otherBlob4 error:nil];

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

    NSString *hash = WebCore::SQLiteFileSystem::computeHashForFileName("StoreBlobToBeDeleted");
    NSString *originDirectory = @"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/IndexedDB/v1/file__0/";
    NSString *databaseDirectory = [[originDirectory stringByAppendingString:hash] stringByExpandingTildeInPath];
    NSString *blobFilePath = [databaseDirectory stringByAppendingPathComponent:@"1.blob"];
    NSString *databaseFilePath = [databaseDirectory stringByAppendingPathComponent:@"IndexedDB.sqlite3"];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:blobFilePath]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:databaseFilePath]);

    // Add a .blob file that is not created by IndexedDB API.
    NSString *anotherBlobFilePath = [databaseDirectory stringByAppendingPathComponent:@"7182.blob"];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:anotherBlobFilePath error:nil];

    readyToContinue = false;
    [webView evaluateJavaScript:@"deleteDatabase(() => { sendMessage('Delete success'); })" completionHandler:nil];
    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Delete success", (NSString *)[lastScriptMessage body]);

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:blobFilePath]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:databaseFilePath]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:anotherBlobFilePath]);
}
