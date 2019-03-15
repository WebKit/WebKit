/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

static bool readyToContinue;
static bool receivedScriptMessage;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface IndexedDBFileNameMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBFileNameMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

static void runTest()
{
    auto handler = adoptNS([[IndexedDBFileNameMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    // Open a database which does not have a database file on disk yet.
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBFileName-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    
    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Success", string.get());

    // Open a database which already has a database file on disk.
    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBFileName-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Success", string2.get());

    // For database that is not opened after upgrade, its file should be in v0.
    auto defaultFileManager = [NSFileManager defaultManager];
    NSString *existingDatabaseName = @"IndexedDBTest";
    NSString *createdDatabaseName = @"IndexedDBOther";
    NSString *unusedDatabaseName = @"IndexedDBUnused";
    NSString *existingDatbaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(existingDatabaseName);
    NSString *createdDatabaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(createdDatabaseName);
    NSURL *idbRootURL = [[WKWebsiteDataStore defaultDataStore] _indexedDBDatabaseDirectory];
    NSURL *oldVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v0"];
    NSURL *newVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v1"];
    NSURL *oldVersionOriginDirectoryURL = [oldVersionDirectoryURL URLByAppendingPathComponent: @"file__0"];
    NSURL *newVersionOriginDirectoryURL = [newVersionDirectoryURL URLByAppendingPathComponent: @"file__0"];
    NSURL *oldVersionUnusedDatabaseDirectoryURL = [oldVersionOriginDirectoryURL URLByAppendingPathComponent:unusedDatabaseName];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:oldVersionUnusedDatabaseDirectoryURL.path]);
    
    // For database that is opened after upgrade, its file should only be in v1.
    NSURL *oldVersionExistingDatabaseDirectoryURL = [oldVersionOriginDirectoryURL URLByAppendingPathComponent: existingDatabaseName];
    NSURL *newVersionExistingDatabaseDirectoryURL = [newVersionOriginDirectoryURL URLByAppendingPathComponent: existingDatbaseHash];
    NSURL *oldVersionCreatedDatabaseDirectoryURL = [oldVersionOriginDirectoryURL URLByAppendingPathComponent: createdDatabaseName];
    NSURL *newVersionCreatedDatabaseDirectoryURL = [newVersionOriginDirectoryURL URLByAppendingPathComponent: createdDatabaseHash];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:oldVersionExistingDatabaseDirectoryURL.path]);
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newVersionExistingDatabaseDirectoryURL.path]);
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:oldVersionCreatedDatabaseDirectoryURL.path]);
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newVersionCreatedDatabaseDirectoryURL.path]);
}

static void createDirectories(String testName)
{
    auto defaultFileManager = [NSFileManager defaultManager];
    NSURL *idbRootURL = [[WKWebsiteDataStore defaultDataStore] _indexedDBDatabaseDirectory];
    [defaultFileManager removeItemAtURL:idbRootURL error:nil];
    
    NSString *existingDatabaseName = @"IndexedDBTest";
    NSString *createdDatabaseName = @"IndexedDBOther";
    NSString *unusedDatabaseName = @"IndexedDBUnused";
    NSURL *directOriginDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"file__0"];
    NSURL *directExistingDatabaseDirectoryURL = [directOriginDirectoryURL URLByAppendingPathComponent:existingDatabaseName];
    NSURL *directUnusedDatabaseDirectoryURL = [directOriginDirectoryURL URLByAppendingPathComponent:unusedDatabaseName];
    NSURL *resourceDatabaseFileURL = [[NSBundle mainBundle] URLForResource:@"IndexedDB" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
    [defaultFileManager createDirectoryAtURL:directExistingDatabaseDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
    [defaultFileManager createDirectoryAtURL:directUnusedDatabaseDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL toURL:[directExistingDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL toURL:[directUnusedDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
    if (testName == "none")
        return;
    
    NSString *existingDatbaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(existingDatabaseName);
    NSString *createdDatabaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(createdDatabaseName);
    NSURL *oldVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v0"];
    NSURL *newVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v1"];
    NSURL *newVersionOriginDirectoryURL = [newVersionDirectoryURL URLByAppendingPathComponent: @"file__0"];
    NSURL *newVersionExistingDatabaseDirectoryURL = [newVersionOriginDirectoryURL URLByAppendingPathComponent: existingDatbaseHash];
    NSURL *newVersionCreatedDatabaseDirectoryURL = [newVersionOriginDirectoryURL URLByAppendingPathComponent: createdDatabaseHash];
    NSURL *directOtherOriginDatabaseDirectoryURL = [[idbRootURL URLByAppendingPathComponent:@"https_apple.com_0"] URLByAppendingPathComponent:@"IndexedDBTest"];
    NSURL *newVersionOtherOriginDatabaseDirectoryURL = [[newVersionDirectoryURL URLByAppendingPathComponent:@"https_webkit.org_0"] URLByAppendingPathComponent:existingDatbaseHash];
    [defaultFileManager createSymbolicLinkAtURL:oldVersionDirectoryURL withDestinationURL:idbRootURL error:nil];

    if (testName == "v0")
        return;
    if (testName == "v1") {
        [defaultFileManager removeItemAtURL:directExistingDatabaseDirectoryURL error:nil];
        [defaultFileManager createDirectoryAtURL:newVersionExistingDatabaseDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
        [defaultFileManager copyItemAtURL:resourceDatabaseFileURL toURL:[newVersionExistingDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
        return;
    }
    if (testName == "API") {
        [defaultFileManager createDirectoryAtURL:directOtherOriginDatabaseDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
        [defaultFileManager createDirectoryAtURL:newVersionOtherOriginDatabaseDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
        [defaultFileManager copyItemAtURL:resourceDatabaseFileURL toURL:[directOtherOriginDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
        [defaultFileManager copyItemAtURL:resourceDatabaseFileURL toURL:[newVersionOtherOriginDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
        return;
    }
    
    [defaultFileManager createDirectoryAtURL:newVersionCreatedDatabaseDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL toURL:[newVersionCreatedDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
}

TEST(IndexedDB, IndexedDBFileName)
{
    createDirectories("none");
    runTest();
}

TEST(IndexedDB, IndexedDBFileNameV0)
{
    createDirectories("v0");
    runTest();
}

TEST(IndexedDB, IndexedDBFileNameV1)
{
    createDirectories("v1");
    runTest();
}

TEST(IndexedDB, IndexedDBFileNameAPI)
{
    createDirectories("API");
    
    RetainPtr<NSSet> types = adoptNS([[NSSet alloc] initWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil]);
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:types.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ(3U, [records count]);
        readyToContinue = true;
    }];
    readyToContinue = false;
    TestWebKitAPI::Util::run(&readyToContinue);

    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    readyToContinue = false;
    TestWebKitAPI::Util::run(&readyToContinue);
    
    auto defaultFileManager = [NSFileManager defaultManager];
    NSURL *idbRootURL = [[WKWebsiteDataStore defaultDataStore] _indexedDBDatabaseDirectory];
    NSURL *newVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v1"];
    NSArray *directories = [defaultFileManager contentsOfDirectoryAtPath:idbRootURL.path error:nullptr];
    EXPECT_EQ(2U, [directories count]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:newVersionDirectoryURL.path]);
    NSArray *newVersionDirectories = [defaultFileManager contentsOfDirectoryAtPath:newVersionDirectoryURL.path error:nullptr];
    EXPECT_EQ(0U, [newVersionDirectories count]);
    
}

TEST(IndexedDB, IndexedDBFileHashCollision)
{
    createDirectories("collision");
    
    auto handler = adoptNS([[IndexedDBFileNameMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBFileName-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Error", string.get());
}
