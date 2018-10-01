/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Deque.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool receivedScriptMessage;
static Deque<RetainPtr<WKScriptMessage>> scriptMessages;

@interface WebsiteDataStoreCustomPathsMessageHandler : NSObject <WKScriptMessageHandler, WKNavigationDelegate>
@end

@implementation WebsiteDataStoreCustomPathsMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessages.append(message);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    // Overwrite the default policy which launches a new web process and reload page on crash.
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

TEST(WebKit, WebsiteDataStoreCustomPaths)
{
    RetainPtr<WebsiteDataStoreCustomPathsMessageHandler> handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    NSURL *sqlPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/WebSQL/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *idbPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *localStoragePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *cookieStorageFile = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/CookieStorage/Cookie.File" stringByExpandingTildeInPath] isDirectory:NO];
    NSURL *resourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

    NSURL *defaultSQLPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/WebSQL/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultIDBPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultLocalStoragePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultResourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

    [[NSFileManager defaultManager] removeItemAtURL:sqlPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:idbPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:localStoragePath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:cookieStorageFile error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:resourceLoadStatisticsPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:defaultSQLPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:defaultIDBPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:defaultLocalStoragePath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:defaultResourceLoadStatisticsPath error:nil];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:sqlPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:localStoragePath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:cookieStorageFile.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:resourceLoadStatisticsPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultSQLPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultIDBPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultLocalStoragePath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    RetainPtr<_WKWebsiteDataStoreConfiguration> websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._webSQLDatabaseDirectory = sqlPath;
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = idbPath;
    websiteDataStoreConfiguration.get()._webStorageDirectory = localStoragePath;
    websiteDataStoreConfiguration.get()._cookieStorageFile = cookieStorageFile;
    websiteDataStoreConfiguration.get()._resourceLoadStatisticsDirectory = resourceLoadStatisticsPath;

    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:handler.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"WebsiteDataStoreCustomPaths" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    EXPECT_FALSE([WKWebsiteDataStore _defaultDataStoreExists]);

    // We expect 4 messages, 1 each for WebSQL, IndexedDB, cookies, and localStorage.
    EXPECT_STREQ([getNextMessage().body UTF8String], "localstorage written");
    EXPECT_STREQ([getNextMessage().body UTF8String], "cookie written");
    EXPECT_STREQ([getNextMessage().body UTF8String], "Exception: QuotaExceededError: The quota has been exceeded.");
    EXPECT_STREQ([getNextMessage().body UTF8String], "Success opening indexed database");

    [[[webView configuration] processPool] _syncNetworkProcessCookies];

    // Forcibly shut down everything of WebKit that we can.
    auto pid = [webView _webProcessIdentifier];
    if (pid)
        kill(pid, SIGKILL);

    webView = nil;
    handler = nil;
    configuration = nil;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:sqlPath.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:localStoragePath.path]);

#if PLATFORM(IOS) || (__MAC_OS_X_VERSION_MIN_REQUIRED < 101300)
    int retryCount = 30;
    while (retryCount--) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:cookieStorageFile.path])
            break;
        TestWebKitAPI::Util::sleep(0.1);
    }
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:cookieStorageFile.path]);

    // Note: The format of the cookie file on disk is proprietary and opaque, so this is fragile to future changes.
    // But right now, it is reliable to scan the file for the ASCII string of the cookie name we set.
    // This helps verify that the cookie file was actually written to as we'd expect.
    auto data = adoptNS([[NSData alloc] initWithContentsOfURL:cookieStorageFile]);
    char bytes[] = "testkey";
    auto cookieKeyData = adoptNS([[NSData alloc] initWithBytes:(void *)bytes length:strlen(bytes)]);
    auto result = [data rangeOfData:cookieKeyData.get() options:0 range:NSMakeRange(0, data.get().length)];
    EXPECT_NE((const long)result.location, NSNotFound);
#endif

#if PLATFORM(MAC)
    // FIXME: The default SQL and LocalStorage paths are being used for something, but they shouldn't be. (theses should be false, not true)
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultSQLPath.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultLocalStoragePath.path]);
#endif

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultIDBPath.path]);
    RetainPtr<NSURL> fileIDBPath = [idbPath URLByAppendingPathComponent:@"file__0"];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:fileIDBPath.get().path]);

    // Data stores can't delete anything unless a WKProcessPool exists, so make sure the shared data store exists.
    auto *processPool = [WKProcessPool _sharedProcessPool];
    RetainPtr<WKWebsiteDataStore> dataStore = [[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()];
    RetainPtr<NSSet> types = adoptNS([[NSSet alloc] initWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil]);

    // Subframe of different origins may also create IndexedDB files.
    RetainPtr<NSURL> url1 = [[NSBundle mainBundle] URLForResource:@"IndexedDB" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> url2 = [[NSBundle mainBundle] URLForResource:@"IndexedDB" withExtension:@"sqlite3-shm" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> url3 = [[NSBundle mainBundle] URLForResource:@"IndexedDB" withExtension:@"sqlite3-wal" subdirectory:@"TestWebKitAPI.resources"];
    
    RetainPtr<NSURL> frameIDBPath = [[fileIDBPath URLByAppendingPathComponent:@"https_apple.com_0"] URLByAppendingPathComponent:@"WebsiteDataStoreCustomPaths"];
    [[NSFileManager defaultManager] createDirectoryAtURL:frameIDBPath.get() withIntermediateDirectories:YES attributes:nil error:nil];
    
    [[NSFileManager defaultManager] copyItemAtURL:url1.get() toURL:[frameIDBPath.get() URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url2.get() toURL:[frameIDBPath.get() URLByAppendingPathComponent:@"IndexedDB.sqlite3-shm"] error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url3.get() toURL:[frameIDBPath.get() URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"] error:nil];
    
    RetainPtr<NSURL> frameIDBPath2 = [[fileIDBPath URLByAppendingPathComponent:@"https_webkit.org_0"] URLByAppendingPathComponent:@"WebsiteDataStoreCustomPaths"];
    [[NSFileManager defaultManager] createDirectoryAtURL:frameIDBPath2.get() withIntermediateDirectories:YES attributes:nil error:nil];
    
    [[NSFileManager defaultManager] copyItemAtURL:url1.get() toURL:[frameIDBPath2.get() URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url2.get() toURL:[frameIDBPath2.get() URLByAppendingPathComponent:@"IndexedDB.sqlite3-shm"] error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url3.get() toURL:[frameIDBPath2.get() URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"] error:nil];

    [dataStore fetchDataRecordsOfTypes:types.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], (unsigned long)3);
        for (id record in records) {
            if ([[record displayName] isEqual: @"apple.com"]) {
                [dataStore removeDataOfTypes:types.get() forDataRecords:[NSArray arrayWithObject:record] completionHandler:^() {
                    receivedScriptMessage = true;
                    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:frameIDBPath.get().path]);
                }];
            }
        }
    }];
    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    [dataStore removeDataOfTypes:types.get() modifiedSince:[NSDate distantPast] completionHandler:[]() {
        receivedScriptMessage = true;
    }];
    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fileIDBPath.get().path]);

    // Now, with brand new WKWebsiteDataStores pointing at the same custom cookie storage location,
    // in newly fired up NetworkProcesses, verify that the fetch and delete APIs work as expected.

    [processPool _terminateNetworkProcess];
    auto newCustomDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    [newCustomDataStore fetchDataRecordsOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeCookies, nil] completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_GT([records count], (unsigned long)0);
        receivedScriptMessage = true;
    }];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    [processPool _terminateNetworkProcess];
    newCustomDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    [newCustomDataStore removeDataOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeCookies, nil] modifiedSince:[NSDate distantPast] completionHandler:^ {
        receivedScriptMessage = true;
    }];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    // This time, reuse the same network process but still do a new websitedatastore, to make sure even an existing network process
    // gets the new datastore.
    newCustomDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    [newCustomDataStore fetchDataRecordsOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeCookies, nil] completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], (unsigned long)0);
        receivedScriptMessage = true;
    }];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    EXPECT_FALSE([WKWebsiteDataStore _defaultDataStoreExists]);
}

TEST(WebKit, CustomDataStorePathsVersusCompletionHandlers)
{
    // Copy the baked database files to the database directory
    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"SimpleServiceWorkerRegistrations-3" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *swPath = [NSURL fileURLWithPath:[@"~/Library/Caches/TestWebKitAPI/WebKit/ServiceWorkers/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] removeItemAtURL:swPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:swPath.path]);

    [[NSFileManager defaultManager] createDirectoryAtURL:swPath withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:[swPath URLByAppendingPathComponent:@"ServiceWorkerRegistrations-3.sqlite3"] error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = swPath;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    // Fetch SW records
    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeServiceWorkerRegistrations]]);
    static bool readyToContinue;
    [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(1U, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    // Fetch records again, this time releasing our reference to the data store while the request is in flight.
    [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(1U, dataRecords.count);
        readyToContinue = true;
    }];
    dataStore = nil;
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    // Delete all SW records, releasing our reference to the data store while the request is in flight.
    dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [dataStore removeDataOfTypes:websiteDataTypes.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    dataStore = nil;
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    // The records should have been deleted, and the callback should have been made.
    // Now refetch the records to verify they are gone.
    dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0U, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WebKit, CustomDataStoreDestroyWhileFetchingNetworkProcessData)
{
    NSURL *cookieStorageFile = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/CookieStorage/Cookie.File" stringByExpandingTildeInPath] isDirectory:NO];
    [[NSFileManager defaultManager] removeItemAtURL:cookieStorageFile error:nil];

    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeCookies]]);
    static bool readyToContinue;

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._cookieStorageFile = cookieStorageFile;

    @autoreleasepool {
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

        // Fetch records
        [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
            EXPECT_EQ((int)dataRecords.count, 0);
            readyToContinue = true;
        }];
        TestWebKitAPI::Util::run(&readyToContinue);
        readyToContinue = false;

        // Fetch records again, this time releasing our reference to the data store while the request is in flight.
        [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
            EXPECT_EQ((int)dataRecords.count, 0);
            readyToContinue = true;
        }];
        dataStore = nil;
    }
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    @autoreleasepool {
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
            EXPECT_EQ((int)dataRecords.count, 0);
            readyToContinue = true;
        }];

        // Terminate the network process while a query is pending.
        auto* allProcessPools = [WKProcessPool _allProcessPoolsForTesting];
        ASSERT_EQ(1U, [allProcessPools count]);
        auto* processPool = allProcessPools[0];
        while (![processPool _networkProcessIdentifier])
            TestWebKitAPI::Util::sleep(0.01);
        kill([processPool _networkProcessIdentifier], SIGKILL);
        allProcessPools = nil;
        dataStore = nil;
    }

    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;
}

TEST(WebKit, WebsiteDataStoreEphemeral)
{
    RetainPtr<WebsiteDataStoreCustomPathsMessageHandler> handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    NSURL *defaultResourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

    [[NSFileManager defaultManager] removeItemAtURL:defaultResourceLoadStatisticsPath error:nil];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration.get().websiteDataStore _setResourceLoadStatisticsEnabled:YES];

    // We expect the directory to be created by starting up the data store machinery, but not the data file.
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    NSURL *defaultResourceLoadStatisticsFilePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/ResourceLoadStatistics/full_browsing_session_resourceLog.plist" stringByExpandingTildeInPath] isDirectory:NO];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsFilePath.path]);

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"WebsiteDataStoreCustomPaths" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    [[[webView configuration] processPool] _syncNetworkProcessCookies];

    // Forcibly shut down everything of WebKit that we can.
    auto pid = [webView _webProcessIdentifier];
    if (pid)
        kill(pid, SIGKILL);

    webView = nil;
    handler = nil;
    configuration = nil;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsFilePath.path]);
}

#endif
