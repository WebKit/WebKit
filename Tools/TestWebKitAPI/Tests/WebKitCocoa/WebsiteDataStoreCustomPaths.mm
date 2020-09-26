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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <JavaScriptCore/JSCConfig.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKPreferencesRef.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Deque.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

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

enum class ShouldEnableProcessPrewarming { No, Yes };

static void runWebsiteDataStoreCustomPaths(ShouldEnableProcessPrewarming shouldEnableProcessPrewarming)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().prewarmsProcessesAutomatically = shouldEnableProcessPrewarming == ShouldEnableProcessPrewarming::Yes ? YES : NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    RetainPtr<WebsiteDataStoreCustomPathsMessageHandler> handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    NSURL *sqlPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/WebSQL/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *idbPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *localStoragePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *cookieStorageFile = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/CookieStorage/Cookie.File" stringByExpandingTildeInPath] isDirectory:NO];
    NSURL *resourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

    NSURL *defaultSQLPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/WebSQL/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultIDBPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultLocalStoragePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultResourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

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
    configuration.get().processPool = processPool.get();

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:handler.get()];

    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetWebSQLDisabled(preferences, false);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"WebsiteDataStoreCustomPaths" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    EXPECT_FALSE([WKWebsiteDataStore _defaultDataStoreExists]);

    // We expect 4 messages, 1 each for WebSQL, IndexedDB, cookies, and localStorage.
    EXPECT_STREQ([getNextMessage().body UTF8String], "localstorage written");
    EXPECT_STREQ([getNextMessage().body UTF8String], "cookie written");
    EXPECT_STREQ([getNextMessage().body UTF8String], "Exception: QuotaExceededError: The quota has been exceeded.");
    EXPECT_STREQ([getNextMessage().body UTF8String], "Success opening indexed database");

    __block bool flushed = false;
    [configuration.get().websiteDataStore.httpCookieStore _flushCookiesToDiskWithCompletionHandler:^{
        flushed = true;
    }];
    TestWebKitAPI::Util::run(&flushed);

    // Forcibly shut down everything of WebKit that we can.
    auto pid = [webView _webProcessIdentifier];
    if (pid)
        kill(pid, SIGKILL);

    webView = nil;
    handler = nil;
    configuration = nil;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:sqlPath.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:localStoragePath.path]);

#if PLATFORM(IOS_FAMILY)
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
    RetainPtr<NSURL> fileIDBPath = [[idbPath URLByAppendingPathComponent:@"v1"] URLByAppendingPathComponent:@"file__0"];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:fileIDBPath.get().path]);

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
                [dataStore removeDataOfTypes:types.get() forDataRecords:@[record] completionHandler:^() {
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

TEST(WebKit, WebsiteDataStoreCustomPathsWithoutPrewarming)
{
    runWebsiteDataStoreCustomPaths(ShouldEnableProcessPrewarming::No);
}

TEST(WebKit, WebsiteDataStoreCustomPathsWithPrewarming)
{
    runWebsiteDataStoreCustomPaths(ShouldEnableProcessPrewarming::Yes);
}

TEST(WebKit, CustomDataStorePathsVersusCompletionHandlers)
{
    // Copy the baked database files to the database directory
    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"SimpleServiceWorkerRegistrations-4" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *swPath = [NSURL fileURLWithPath:[@"~/Library/Caches/com.apple.WebKit.TestWebKitAPI/WebKit/ServiceWorkers/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] removeItemAtURL:swPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:swPath.path]);

    [[NSFileManager defaultManager] createDirectoryAtURL:swPath withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:[swPath URLByAppendingPathComponent:@"ServiceWorkerRegistrations-5.sqlite3"] error:nil];

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
    NSURL *cookieStorageFile = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/CookieStorage/Cookie.File" stringByExpandingTildeInPath] isDirectory:NO];
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

    NSURL *defaultResourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

    [[NSFileManager defaultManager] removeItemAtURL:defaultResourceLoadStatisticsPath error:nil];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration.get().websiteDataStore _setResourceLoadStatisticsEnabled:YES];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    NSURL *defaultResourceLoadStatisticsFilePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics/full_browsing_session_resourceLog.plist" stringByExpandingTildeInPath] isDirectory:NO];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsFilePath.path]);

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"WebsiteDataStoreCustomPaths" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    __block bool flushed = false;
    [configuration.get().websiteDataStore.httpCookieStore _flushCookiesToDiskWithCompletionHandler:^{
        flushed = true;
    }];
    TestWebKitAPI::Util::run(&flushed);

    // Forcibly shut down everything of WebKit that we can.
    auto pid = [webView _webProcessIdentifier];
    if (pid)
        kill(pid, SIGKILL);

    webView = nil;
    handler = nil;
    configuration = nil;

    [WKWebsiteDataStore defaultDataStore];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsFilePath.path]);

    [[NSFileManager defaultManager] removeItemAtURL:defaultResourceLoadStatisticsPath error:nil];
}

TEST(WebKit, AlternativeServicesDefaultDirectoryCreation)
{
    NSURL *defaultDirectory = [NSURL fileURLWithPath:[@"~/Library/Caches/com.apple.WebKit.TestWebKitAPI/WebKit/AlternativeServices" stringByExpandingTildeInPath] isDirectory:YES];
    [[NSFileManager defaultManager] removeItemAtURL:defaultDirectory error:nil];
    
    TestWKWebView *webView1 = [[[TestWKWebView alloc] init] autorelease];
    [webView1 synchronouslyLoadHTMLString:@"start auxiliary processes" baseURL:nil];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultDirectory.path]);
    
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)

#if PLATFORM(MAC)
    NSString *key = @"ExperimentalHTTP3Enabled";
#else
    NSString *key = @"WebKitExperimentalHTTP3Enabled";
#endif
    
    [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithBool:YES] forKey:key];

    TestWKWebView *webView2 = [[[TestWKWebView alloc] init] autorelease];
    [webView2 synchronouslyLoadHTMLString:@"start auxiliary processes" baseURL:nil];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultDirectory.path]);

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:key];
    [[NSFileManager defaultManager] removeItemAtURL:defaultDirectory error:nil];

#endif // HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
}

TEST(WebKit, WebsiteDataStoreEphemeralViaConfiguration)
{
    RetainPtr<WebsiteDataStoreCustomPathsMessageHandler> handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    NSURL *defaultResourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

    [[NSFileManager defaultManager] removeItemAtURL:defaultResourceLoadStatisticsPath error:nil];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    RetainPtr<_WKWebsiteDataStoreConfiguration> dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()] autorelease];
    [configuration.get().websiteDataStore _setResourceLoadStatisticsEnabled:YES];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    NSURL *defaultResourceLoadStatisticsFilePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics/full_browsing_session_resourceLog.plist" stringByExpandingTildeInPath] isDirectory:NO];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsFilePath.path]);

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"WebsiteDataStoreCustomPaths" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    __block bool flushed = false;
    [configuration.get().websiteDataStore.httpCookieStore _flushCookiesToDiskWithCompletionHandler:^{
        flushed = true;
    }];
    TestWebKitAPI::Util::run(&flushed);

    // Forcibly shut down everything of WebKit that we can.
    auto pid = [webView _webProcessIdentifier];
    if (pid)
        kill(pid, SIGKILL);

    webView = nil;
    handler = nil;
    configuration = nil;

    [WKWebsiteDataStore defaultDataStore];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsFilePath.path]);

    [[NSFileManager defaultManager] removeItemAtURL:defaultResourceLoadStatisticsPath error:nil];
}

TEST(WebKit, DoLoadWithNonDefaultDataStoreAfterTerminatingNetworkProcess)
{
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    webViewConfiguration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    TestWebKitAPI::Util::spinRunLoop(1);

    [webViewConfiguration.get().processPool _terminateNetworkProcess];

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
}

TEST(WebKit, WebsiteDataStoreConfigurationPathNull)
{
    EXPECT_TRUE([[[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease] _indexedDBDatabaseDirectory]);
    EXPECT_FALSE([[[[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration] autorelease] _indexedDBDatabaseDirectory]);
}

TEST(WebKit, WebsiteDataStoreIfExists)
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_FALSE([webViewConfiguration _websiteDataStoreIfExists]);
    WKWebsiteDataStore *dataStore = [webViewConfiguration websiteDataStore];
    EXPECT_TRUE([webViewConfiguration _websiteDataStoreIfExists]);
    EXPECT_TRUE(dataStore._configuration.persistent);
}

TEST(WebKit, WebsiteDataStoreRenameOriginForIndexedDatabase)
{
    // Reset defaultDataStore before test.
    __block bool done = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSString *testString = @"<script> \
        var request = window.indexedDB.open('testDB'); \
        var db; \
        request.onupgradeneeded = function(event) { \
            db = event.target.result; \
            window.webkit.messageHandlers.testHandler.postMessage('database is created'); \
            db.onclose = function(event) { \
                window.webkit.messageHandlers.testHandler.postMessage('database is closed'); \
            } \
        }; \
        request.onsuccess = function(event) { \
            if (!db) { \
                db = event.target.result; \
                window.webkit.messageHandlers.testHandler.postMessage('database exists'); \
            } \
        }; \
        request.onerror = function(event) { \
            if (!db) { \
                window.webkit.messageHandlers.testHandler.postMessage('database error: ' + event.target.error.name + ' - ' + event.target.error.message); \
            } \
        }; \
        </script>";

    NSURL *exampleURL = [NSURL URLWithString:@"http://example.com/"];
    NSURL *webKitURL = [NSURL URLWithString:@"https://webkit.org/"];

    [webView loadHTMLString:testString baseURL:exampleURL];
    EXPECT_WK_STREQ("database is created", getNextMessage().body);

    auto dataStore = webView.get().configuration.websiteDataStore;
    auto indexedDBType = adoptNS([[NSSet alloc] initWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil]);
    [dataStore _renameOrigin:exampleURL to:webKitURL forDataOfTypes:indexedDBType.get() completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_WK_STREQ("database is closed", getNextMessage().body);
    
    [webView loadHTMLString:testString baseURL:webKitURL];
    EXPECT_WK_STREQ("database exists", getNextMessage().body);

    [webView loadHTMLString:testString baseURL:exampleURL];
    EXPECT_WK_STREQ("database is created", getNextMessage().body);

    // Clean up defaultDataStore after test.
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, WebsiteDataStoreRenameOrigin)
{
    TestWKWebView *webView = [[[TestWKWebView alloc] init] autorelease];
    [webView synchronouslyLoadHTMLString:@"<script>localStorage.setItem('testkey', 'testvalue')</script>" baseURL:[NSURL URLWithString:@"https://example.com/"]];
    
    __block bool done = false;
    NSURL *exampleURL = [NSURL URLWithString:@"https://example.com/"];
    NSURL *webKitURL = [NSURL URLWithString:@"https://webkit.org/"];
    WKWebsiteDataStore *dataStore = webView.configuration.websiteDataStore;
    NSSet *localStorageSet = [NSSet setWithObject:WKWebsiteDataTypeLocalStorage];
    [dataStore _renameOrigin:exampleURL to:webKitURL forDataOfTypes:localStorageSet completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView synchronouslyLoadHTMLString:@"hello" baseURL:webKitURL];
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"localStorage.getItem('testkey')"], "testvalue");
    [webView synchronouslyLoadHTMLString:@"hello" baseURL:exampleURL];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"localStorage.getItem('testkey')"] isKindOfClass:[NSNull class]]);

    done = false;
    [dataStore fetchDataRecordsOfTypes:localStorageSet completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        [dataStore removeDataOfTypes:localStorageSet forDataRecords:records completionHandler:^{
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, NetworkCacheDirectory)
{
    using namespace TestWebKitAPI;
    TCPServer server([] (int socket) {
        TCPServer::read(socket);
        const char* response =
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=1000000\r\n"
        "Content-Length: 6\r\n\r\n"
        "Hello!";
        TCPServer::write(socket, response, strlen(response));
    });
    
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"CustomPathsTest"] isDirectory:YES];
    
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [websiteDataStoreConfiguration setNetworkCacheDirectory:tempDir];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease]];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    NSString *path = tempDir.path;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    while (![fileManager fileExistsAtPath:path])
        Util::spinRunLoop();
    NSError *error = nil;
    [fileManager removeItemAtPath:path error:&error];
    EXPECT_FALSE(error);
}

TEST(WebKit, ApplicationCacheDirectories)
{
    TestWebKitAPI::HTTPServer server({
        { "/index.html", { "<html manifest='test.appcache'>" } },
        { "/test.appcache", { "CACHE MANIFEST\nindex.html\ntest.mp4\n" } },
        { "/test.mp4", { {{ "Content-Type", "video/test" }}, "test!" }},
    });
    
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"CustomPathsTest"] isDirectory:YES];
    NSString *path = tempDir.path;
    NSString *subdirectoryPath = [path stringByAppendingPathComponent:@"testsubdirectory"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    EXPECT_FALSE([fileManager fileExistsAtPath:subdirectoryPath]);

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);

    [websiteDataStoreConfiguration setApplicationCacheDirectory:tempDir];
    [websiteDataStoreConfiguration setApplicationCacheFlatFileSubdirectoryName:@"testsubdirectory"];
    
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease]];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/index.html", server.port()]]]];

    while (![fileManager fileExistsAtPath:subdirectoryPath])
        TestWebKitAPI::Util::spinRunLoop();

    NSError *error = nil;
    [fileManager removeItemAtPath:path error:&error];
    EXPECT_FALSE(error);
}

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)

static void checkUntilEntryFound(WKWebsiteDataStore *dataStore, void(^completionHandler)(NSArray<WKWebsiteDataRecord *> *))
{
    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeAlternativeServices] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        if (records.count)
            completionHandler(records);
        else
            checkUntilEntryFound(dataStore, completionHandler);
    }];
}

// This test needs to remain disabled until rdar://problem/59644683 is resolved.
TEST(WebKit, DISABLED_AlternativeService)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/", { {{ "alt-svc", "h3-24=\":443\"; ma=3600; persist=1" }}, "<html>test content</html>" } }
    }, HTTPServer::Protocol::Https);

    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"AlternativeServiceTest"] isDirectory:YES];
    NSString *path = [tempDir URLByAppendingPathComponent:@"AlternativeService.sqlite"].path;
    NSError *error = nil;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtPath:path error:&error];

    _WKWebsiteDataStoreConfiguration *dataStoreConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    dataStoreConfiguration.alternativeServicesStorageDirectory = tempDir;
    WKWebsiteDataStore *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration] autorelease];

    __block bool done = false;
    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeAlternativeServices] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 0u);
        done = true;
    }];
    Util::run(&done);

    WKWebViewConfiguration *webViewConfiguration = [[[WKWebViewConfiguration alloc] init] autorelease];
    webViewConfiguration.websiteDataStore = dataStore;
    WKWebView *webView = [[[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration] autorelease];
    TestNavigationDelegate *delegate = [[[TestNavigationDelegate alloc] init] autorelease];
    webView.navigationDelegate = delegate;
    delegate.didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };
    
    [webView loadRequest:server.request()];

    done = false;
    checkUntilEntryFound(dataStore, ^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 1u);
        [dataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeAlternativeServices] forDataRecords:records completionHandler:^{
            [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeAlternativeServices] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
                EXPECT_EQ(records.count, 0u);
                done = true;
            }];
        }];
    });
    Util::run(&done);
    
    EXPECT_TRUE([fileManager fileExistsAtPath:path]);
    // We can't unlink the sqlite file because it is guarded by the network process right now.
    // We delete it before running this test the next time.
}

#endif // HAVE(CFNETWORK_ALTERNATIVE_SERVICE)

TEST(WebKit, MediaCache)
{
    JSC::Config::configureForTesting();

    std::atomic<bool> done = false;
    using namespace TestWebKitAPI;
    RetainPtr<NSData> data = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"]];
    uint64_t dataLength = [data length];

    TCPServer server([&] (int socket) {
        TCPServer::read(socket);
        const char* firstResponse =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 55\r\n\r\n"
        "<video><source src='test.mp4' type='video/mp4'></video>";
        TCPServer::write(socket, firstResponse, strlen(firstResponse));

        while (!done) {
            auto bytes = TCPServer::read(socket);
            if (done || bytes.isEmpty())
                break;
            StringView request(static_cast<const LChar*>(bytes.data()), bytes.size());
            String rangeBytes = "Range: bytes="_s;
            auto begin = request.find(StringView(rangeBytes), 0);
            ASSERT(begin != notFound);
            auto dash = request.find('-', begin);
            ASSERT(dash != notFound);
            auto end = request.find('\r', dash);
            ASSERT(end != notFound);

            auto rangeBegin = *request.substring(begin + rangeBytes.length(), dash - begin - rangeBytes.length()).toUInt64Strict();
            auto rangeEnd = *request.substring(dash + 1, end - dash - 1).toUInt64Strict();

            NSString *responseHeaderString = [NSString stringWithFormat:
                @"HTTP/1.1 206 Partial Content\r\n"
                "Content-Range: bytes %llu-%llu/%llu\r\n"
                "Content-Length: %llu\r\n\r\n",
                rangeBegin, rangeEnd, dataLength, rangeEnd - rangeBegin];
            NSData *responseHeader = [responseHeaderString dataUsingEncoding:NSUTF8StringEncoding];
            NSData *responseBody = [data subdataWithRange:NSMakeRange(rangeBegin, rangeEnd - rangeBegin)];
            NSMutableData *response = [NSMutableData dataWithCapacity:responseHeader.length + responseBody.length];
            [response appendData:responseHeader];
            [response appendData:responseBody];
            TCPServer::write(socket, response.bytes, response.length);
        }
    });

    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"CustomPathsTest"] isDirectory:YES];
    NSString *path = tempDir.path;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    EXPECT_FALSE([fileManager fileExistsAtPath:path]);

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [websiteDataStoreConfiguration setMediaCacheDirectory:tempDir];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease]];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

    NSError *error = nil;
    while (![fileManager contentsOfDirectoryAtPath:path error:&error].count)
        Util::spinRunLoop();
    EXPECT_FALSE(error);

    done = true;
    [[webView configuration].processPool _terminateNetworkProcess];

    [fileManager removeItemAtPath:path error:&error];
    EXPECT_FALSE(error);
}
