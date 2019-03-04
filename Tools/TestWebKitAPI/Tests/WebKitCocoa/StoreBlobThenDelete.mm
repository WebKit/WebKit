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
#import <WebKit/WebKit.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

static bool readyToContinue;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface StoreBlobMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation StoreBlobMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    readyToContinue = true;
    lastScriptMessage = message;
}

@end

TEST(IndexedDB, StoreBlobThenDelete)
{
    RetainPtr<StoreBlobMessageHandler> handler = adoptNS([[StoreBlobMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"StoreBlobToBeDeleted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&readyToContinue);
    EXPECT_WK_STREQ(@"Success", (NSString *)[lastScriptMessage body]);

    NSString *blobFilePath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/StoreBlobToBeDeleted/1.blob" stringByExpandingTildeInPath];
    NSString *databaseFilePath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/StoreBlobToBeDeleted/IndexedDB.sqlite3" stringByExpandingTildeInPath];

    // The database file and blob file should definitely be there right now.
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:blobFilePath]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:databaseFilePath]);

    // To make sure that the -wal and -shm files for a database are deleted even if the sqlite3 file is already missing,
    // we need to:
    // 1 - Create the path for a fake database that won't actively be in use
    // 2 - Move -wal and -shm files into that directory
    // 3 - Make sure the entire directory is deleted
    NSString *fakeDatabasePath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/FakeDatabasePath" stringByExpandingTildeInPath];
    NSString *fakeShmPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/FakeDatabasePath/IndexedDB.sqlite3-wal" stringByExpandingTildeInPath];
    NSString *fakeWalPath = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/FakeDatabasePath/IndexedDB.sqlite3-shm" stringByExpandingTildeInPath];
    [[NSFileManager defaultManager] createDirectoryAtPath:fakeDatabasePath withIntermediateDirectories:NO attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:databaseFilePath toPath:fakeShmPath error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:databaseFilePath toPath:fakeWalPath error:nil];

    // Make some other .blob files in the database directory to later validate that only appropriate files are deleted.
    NSString *otherBlob1 = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/StoreBlobToBeDeleted/7182.blob" stringByExpandingTildeInPath];
    NSString *otherBlob2 = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/StoreBlobToBeDeleted/1a.blob" stringByExpandingTildeInPath];
    NSString *otherBlob3 = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/StoreBlobToBeDeleted/a1.blob" stringByExpandingTildeInPath];
    NSString *otherBlob4 = [@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/file__0/StoreBlobToBeDeleted/.blob" stringByExpandingTildeInPath];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob1 error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob2 error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob3 error:nil];
    [[NSFileManager defaultManager] copyItemAtPath:blobFilePath toPath:otherBlob4 error:nil];

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:blobFilePath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:databaseFilePath]);

        // Make sure the fake blob file with a valid blob file name is gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob1]);

        // Make sure the fake blob files with invalid blob file names are still there.
        EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob2]);
        EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob3]);
        EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:otherBlob4]);

        // Make sure everything related to the fake database is gone.
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeShmPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeWalPath]);
        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fakeDatabasePath]);

        // Now delete them so we're not leaving files around.
        [[NSFileManager defaultManager] removeItemAtPath:otherBlob2 error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:otherBlob3 error:nil];
        [[NSFileManager defaultManager] removeItemAtPath:otherBlob4 error:nil];

        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}
