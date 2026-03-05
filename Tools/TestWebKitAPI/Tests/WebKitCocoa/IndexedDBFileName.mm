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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import <WebCore/SQLiteFileSystem.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

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
    // We need to create custom WebsiteDataStore that uses custom IndexedDB paths to test old migration code.
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get().unifiedOriginStorageLevel = _WKUnifiedOriginStorageLevelNone;
    // Custom WebsiteDataStore must have a different general storage directory than default WebsiteDataStore.
    websiteDataStoreConfiguration.get().generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    // Open a database which does not have a database file on disk yet.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBFileName-1" withExtension:@"html"]];
    [webView loadRequest:request];
    
    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Success", string.get());

    // Open a database which already has a database file on disk.
    request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBFileName-2" withExtension:@"html"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Success", string2.get());

    // All databases of an origin are migrated to new version directory when the origin is accessed.
    auto defaultFileManager = [NSFileManager defaultManager];
    NSString *existingDatabaseName = @"IndexedDBTest";
    NSString *createdDatabaseName = @"IndexedDBOther";
    NSString *unusedDatabaseName = @"IndexedDBUnused";
    RetainPtr existingDatbaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(String { existingDatabaseName }).createNSString();
    RetainPtr createdDatabaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(String { createdDatabaseName }).createNSString();
    RetainPtr unusedDatabaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(String { createdDatabaseName }).createNSString();
    NSURL *idbRootURL = [websiteDataStoreConfiguration.get() _indexedDBDatabaseDirectory];
    NSURL *oldVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v0"];
    NSURL *newVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v1"];
    NSURL *oldVersionOriginDirectoryURL = [oldVersionDirectoryURL URLByAppendingPathComponent: @"file__0"];
    NSURL *newVersionOriginDirectoryURL = [newVersionDirectoryURL URLByAppendingPathComponent: @"file__0"];
    NSURL *oldVersionUnusedDatabaseDirectoryURL = [oldVersionOriginDirectoryURL URLByAppendingPathComponent:unusedDatabaseName];
    NSURL *newVersionUnusedDatabaseDirectoryURL = [newVersionOriginDirectoryURL URLByAppendingPathComponent:unusedDatabaseHash.get()];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:oldVersionUnusedDatabaseDirectoryURL.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:newVersionUnusedDatabaseDirectoryURL.path]);
    
    // For database that is opened after upgrade, its file should only be in v1.
    NSURL *oldVersionExistingDatabaseDirectoryURL = [oldVersionOriginDirectoryURL URLByAppendingPathComponent: existingDatabaseName];
    NSURL *newVersionExistingDatabaseDirectoryURL = [newVersionOriginDirectoryURL URLByAppendingPathComponent: existingDatbaseHash.get()];
    NSURL *oldVersionCreatedDatabaseDirectoryURL = [oldVersionOriginDirectoryURL URLByAppendingPathComponent: createdDatabaseName];
    NSURL *newVersionCreatedDatabaseDirectoryURL = [newVersionOriginDirectoryURL URLByAppendingPathComponent: createdDatabaseHash.get()];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:oldVersionExistingDatabaseDirectoryURL.path]);
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newVersionExistingDatabaseDirectoryURL.path]);
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:oldVersionCreatedDatabaseDirectoryURL.path]);
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newVersionCreatedDatabaseDirectoryURL.path]);
}

static void createDirectories(StringView testName)
{
    auto defaultFileManager = [NSFileManager defaultManager];
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    RetainPtr idbRootURL = [websiteDataStoreConfiguration _indexedDBDatabaseDirectory];
    NSError *error = nil;
    [defaultFileManager removeItemAtURL:idbRootURL.get() error:&error];
    EXPECT_NULL(error);

    /*
     IndexedDB directory structure for non-unified origin storage:
     |- IndexedDB
     |  |- Origin
     |      |- DatabaseName
     |          |- IndexedDB.sqlite3
     |  |- v0 // Symlink to its parent IndexedDB
     |  |- v1
     |      |- Origin
     |          |- DatabaseNameHash
     |              | - IndexedDB.sqlite3
     This function creates directories and files for different test cases:
        - none: IndexedDB directory only contains Origin directories.
        - v0: IndexedDB directory contains Origin and v0 directories.
        - v1: IndexedDB directory conatins Origin, v0 and v1 directories (existing database is in v1 directory).
        - API: IndexedDB directory contains Origin, v0, and v1 directories; v1 directory has multiple Origin directories.
        - collision: Same as API; plus a database file with wrong database name inside, to simulate hash collision case.
     */
    // Baked database file which contains database name "IndexedDBTest".
    RetainPtr resourceDatabaseFileURL = [NSBundle.test_resourcesBundle URLForResource:@"IndexedDB" withExtension:@"sqlite3"];
    // Existing database to be opened by test page.
    RetainPtr existingDatabaseName = @"IndexedDBTest";
    String existingDatabaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(String { existingDatabaseName.get() });
    // Database to be created by test page.
    RetainPtr createdDatabaseName = @"IndexedDBOther";
    String createdDatabaseHash = WebCore::SQLiteFileSystem::computeHashForFileName(String { createdDatabaseName.get() });
    // Existing database that test page does not use at all; only used for verify data migration.
    RetainPtr unusedDatabaseName = @"IndexedDBUnused";
    RetainPtr fileOrigin = @"file__0";
    RetainPtr oldVersion = @"v0";
    RetainPtr newVersion = @"v1";

    RetainPtr existingDatabaseDirectoryURL = [NSURL fileURLWithPath:[NSString pathWithComponents:@[idbRootURL.get().path, fileOrigin.get(), existingDatabaseName.get()]]];
    [defaultFileManager createDirectoryAtURL:existingDatabaseDirectoryURL.get() withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_NULL(error);
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:[existingDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:&error];
    EXPECT_NULL(error);
    RetainPtr unusedDatabaseDirectoryURL = [NSURL fileURLWithPath:[NSString pathWithComponents:@[idbRootURL.get().path, fileOrigin.get(), unusedDatabaseName.get()]]];
    [defaultFileManager createDirectoryAtURL:unusedDatabaseDirectoryURL.get() withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_NULL(error);
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:[unusedDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:&error];
    EXPECT_NULL(error);
    if (testName == "none"_s)
        return;
    
    RetainPtr oldVersionDirectoryURL = [idbRootURL.get() URLByAppendingPathComponent:oldVersion.get()];
    [defaultFileManager createSymbolicLinkAtURL:oldVersionDirectoryURL.get() withDestinationURL:idbRootURL.get() error:&error];
    EXPECT_NULL(error);
    if (testName == "v0"_s)
        return;

    [defaultFileManager removeItemAtURL:existingDatabaseDirectoryURL.get() error:nil];
    RetainPtr v1ExistingDatabaseDirectoryURL = [NSURL fileURLWithPath:[NSString pathWithComponents:@[idbRootURL.get().path, newVersion.get(), fileOrigin.get(), existingDatabaseHash.createNSString().get()]]];
    [defaultFileManager createDirectoryAtURL:v1ExistingDatabaseDirectoryURL.get() withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_NULL(error);
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:[v1ExistingDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:&error];
    EXPECT_NULL(error);
    if (testName == "v1"_s)
        return;

    RetainPtr appleDatabaseDirectoryURL = [NSURL fileURLWithPath:[NSString pathWithComponents:@[idbRootURL.get().path, @"https_apple.com_0", existingDatabaseName.get()]]];
    [defaultFileManager createDirectoryAtURL:appleDatabaseDirectoryURL.get() withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_NULL(error);
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:[appleDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:&error];
    EXPECT_NULL(error);
    RetainPtr v1WebKitOriginDatabaseDirectoryURL = [NSURL fileURLWithPath:[NSString pathWithComponents:@[idbRootURL.get().path, newVersion.get(), @"https_webkit.org_0", existingDatabaseHash.createNSString().get()]]];
    [defaultFileManager createDirectoryAtURL:v1WebKitOriginDatabaseDirectoryURL.get() withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_NULL(error);
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:[v1WebKitOriginDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:&error];
    EXPECT_NULL(error);
    if (testName == "API"_s)
        return;

    EXPECT_WK_STREQ("collision", testName.utf8().data());
    RetainPtr v1CreatedDatabaseDirectoryURL = [NSURL fileURLWithPath:[NSString pathWithComponents:@[idbRootURL.get().path, newVersion.get(), fileOrigin.get(), createdDatabaseHash.createNSString().get()]]];
    [defaultFileManager createDirectoryAtURL:v1CreatedDatabaseDirectoryURL.get() withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_NULL(error);
    [defaultFileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:[v1CreatedDatabaseDirectoryURL URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:&error];
    EXPECT_NULL(error);
}

TEST(IndexedDB, IndexedDBFileName)
{
    createDirectories("none"_s);
    runTest();
}

TEST(IndexedDB, IndexedDBFileNameV0)
{
    createDirectories("v0"_s);
    runTest();
}

TEST(IndexedDB, IndexedDBFileNameV1)
{
    createDirectories("v1"_s);
    runTest();
}

TEST(IndexedDB, IndexedDBFileNameAPI)
{
    createDirectories("API"_s);
    
    auto types = adoptNS([[NSSet alloc] initWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get().unifiedOriginStorageLevel = _WKUnifiedOriginStorageLevelNone;
    websiteDataStoreConfiguration.get().generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    [dataStore fetchDataRecordsOfTypes:types.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ(3U, [records count]);
        readyToContinue = true;
    }];
    readyToContinue = false;
    TestWebKitAPI::Util::run(&readyToContinue);

    [dataStore removeDataOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    readyToContinue = false;
    TestWebKitAPI::Util::run(&readyToContinue);
    
    auto defaultFileManager = [NSFileManager defaultManager];
    NSURL *idbRootURL = [[[WKWebsiteDataStore defaultDataStore] _configuration] _indexedDBDatabaseDirectory];
    NSURL *newVersionDirectoryURL = [idbRootURL URLByAppendingPathComponent:@"v1"];
    NSArray *directories = [defaultFileManager contentsOfDirectoryAtPath:idbRootURL.path error:nullptr];
    EXPECT_EQ(2U, [directories count]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:newVersionDirectoryURL.path]);
    NSArray *newVersionDirectories = [defaultFileManager contentsOfDirectoryAtPath:newVersionDirectoryURL.path error:nullptr];
    EXPECT_EQ(0U, [newVersionDirectories count]);
    
}

TEST(IndexedDB, IndexedDBFileHashCollision)
{
    createDirectories("collision"_s);
    
    auto handler = adoptNS([[IndexedDBFileNameMessageHandler alloc] init]);
    RetainPtr websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get().unifiedOriginStorageLevel = _WKUnifiedOriginStorageLevelNone;
    websiteDataStoreConfiguration.get().generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBFileName-1" withExtension:@"html"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr<NSString> string = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Error", string.get());
}

TEST(IndexedDB, OpenDatabaseAfterDatabaseNameUpgrade)
{
    RetainPtr handler = adoptNS([[IndexedDBFileNameMessageHandler alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr originURL = [NSURL URLWithString:@"file://"];
    __block RetainPtr<NSString> indexedDBDirectoryString;
    __block bool done = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL.get() topOrigin:originURL.get() type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        indexedDBDirectoryString = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    RetainPtr indexedDBDirectory = [NSURL fileURLWithPath:indexedDBDirectoryString.get() isDirectory:YES];
    String databaseHash = WebCore::SQLiteFileSystem::computeHashForFileName("IndexedDBTest"_s);
    RetainPtr indexedDBDatabaseDirectory = [indexedDBDirectory URLByAppendingPathComponent:databaseHash.createNSString().get()];
    RetainPtr indexedDBDatabaseFile = [indexedDBDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];
    RetainPtr resourceDatabaseFileURL = [NSBundle.test_resourcesBundle URLForResource:@"IndexedDB" withExtension:@"sqlite3"];
    RetainPtr fileManager = [NSFileManager defaultManager];
    // Remove existing database files.
    [fileManager removeItemAtURL:indexedDBDatabaseDirectory.get() error:nil];
    [fileManager createDirectoryAtURL:indexedDBDatabaseDirectory.get() withIntermediateDirectories:YES attributes:nil error:nil];
    // Create database file with old version.
    [fileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:indexedDBDatabaseFile.get() error:nil];
    EXPECT_TRUE([fileManager fileExistsAtPath:resourceDatabaseFileURL.get().path]);

    // Upgrade an existing database.
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBFileName-2" withExtension:@"html"]];
    receivedScriptMessage = false;
    [webView loadRequest:request.get()];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr string = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Success", string.get());

    // Force re-opening database.
    [configuration.get().websiteDataStore _terminateNetworkProcess];

    // Ensure the upgraded database can be opened.
    receivedScriptMessage = false;
    [webView reload];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    string = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Success", string.get());
}

TEST(IndexedDB, OpenDatabaseWithMismatchedMetadataVersionAndName)
{
    RetainPtr handler = adoptNS([[IndexedDBFileNameMessageHandler alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr originURL = [NSURL URLWithString:@"file://"];
    __block RetainPtr<NSString> indexedDBDirectoryString;
    __block bool done = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:originURL.get() topOrigin:originURL.get() type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        indexedDBDirectoryString = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    RetainPtr indexedDBDirectory = [NSURL fileURLWithPath:indexedDBDirectoryString.get() isDirectory:YES];
    String databaseHash = WebCore::SQLiteFileSystem::computeHashForFileName("IndexedDBTest"_s);
    RetainPtr indexedDBDatabaseDirectory = [indexedDBDirectory URLByAppendingPathComponent:databaseHash.createNSString().get()];
    RetainPtr indexedDBDatabaseFile = [indexedDBDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];
    RetainPtr resourceDatabaseFileURL = [NSBundle.test_resourcesBundle URLForResource:@"IndexedDBWrongMetadataVersion" withExtension:@"sqlite3"];
    RetainPtr fileManager = [NSFileManager defaultManager];
    // Remove existing database files.
    [fileManager removeItemAtURL:indexedDBDatabaseDirectory.get() error:nil];
    [fileManager createDirectoryAtURL:indexedDBDatabaseDirectory.get() withIntermediateDirectories:YES attributes:nil error:nil];
    // Copy baked database files to IndexedDB directory.
    [fileManager copyItemAtURL:resourceDatabaseFileURL.get() toURL:indexedDBDatabaseFile.get() error:nil];
    EXPECT_TRUE([fileManager fileExistsAtPath:resourceDatabaseFileURL.get().path]);

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBFileName-2" withExtension:@"html"]];
    receivedScriptMessage = false;
    [webView loadRequest:request.get()];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    RetainPtr string = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"Success", string.get());
}
