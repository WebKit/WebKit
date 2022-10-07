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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <JavaScriptCore/JSCConfig.h>
#import <WebCore/SQLiteFileSystem.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
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
#import <wtf/text/StringToIntegerConversion.h>
#import <wtf/text/WTFString.h>

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

enum class ShouldEnableProcessPrewarming { No, Yes };

static void runWebsiteDataStoreCustomPaths(ShouldEnableProcessPrewarming shouldEnableProcessPrewarming)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().prewarmsProcessesAutomatically = shouldEnableProcessPrewarming == ShouldEnableProcessPrewarming::Yes ? YES : NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    NSURL *idbPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *localStoragePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *cookieStorageFile = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/CookieStorage/Cookie.File" stringByExpandingTildeInPath] isDirectory:NO];
    NSURL *resourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultIDBPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultLocalStoragePath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *defaultResourceLoadStatisticsPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics/" stringByExpandingTildeInPath] isDirectory:YES];

    [[NSFileManager defaultManager] removeItemAtURL:idbPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:localStoragePath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:cookieStorageFile error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:resourceLoadStatisticsPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:defaultIDBPath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:defaultLocalStoragePath error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:defaultResourceLoadStatisticsPath error:nil];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:localStoragePath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:cookieStorageFile.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:resourceLoadStatisticsPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultIDBPath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultLocalStoragePath.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultResourceLoadStatisticsPath.path]);

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = true;
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = idbPath;
    websiteDataStoreConfiguration.get()._webStorageDirectory = localStoragePath;
    websiteDataStoreConfiguration.get()._cookieStorageFile = cookieStorageFile;
    websiteDataStoreConfiguration.get()._resourceLoadStatisticsDirectory = resourceLoadStatisticsPath;

    configuration.get().websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get();
    configuration.get().processPool = processPool.get();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:handler.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"WebsiteDataStoreCustomPaths" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    EXPECT_FALSE([WKWebsiteDataStore _defaultDataStoreExists]);

    // We expect 3 messages, 1 each for IndexedDB, cookies, and localStorage.
    EXPECT_STREQ([getNextMessage().body UTF8String], "localstorage written");
    EXPECT_STREQ([getNextMessage().body UTF8String], "cookie written");
    EXPECT_STREQ([getNextMessage().body UTF8String], "Success opening indexed database");

    __block bool flushed = false;
    [configuration.get().websiteDataStore.httpCookieStore _flushCookiesToDiskWithCompletionHandler:^{
        flushed = true;
    }];
    TestWebKitAPI::Util::run(&flushed);

    // Get the IndexedDB database path in use.
    NSURL *fileURL = [NSURL URLWithString:@"file://"];
    __block NSString *fileIDBPathString = nil;
    done = false;
    [configuration.get().websiteDataStore _originDirectoryForTesting:fileURL topOrigin:fileURL type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
        fileIDBPathString = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    NSURL *fileIDBPath = [NSURL fileURLWithPath:fileIDBPathString isDirectory:YES];

    // Forcibly shut down everything of WebKit that we can.
    auto pid = [webView _webProcessIdentifier];
    if (pid)
        kill(pid, SIGKILL);

    webView = nil;
    handler = nil;
    configuration = nil;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:localStoragePath.path]);

#if PLATFORM(IOS_FAMILY)
    int retryCount = 30;
    while (retryCount--) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:cookieStorageFile.path])
            break;
        TestWebKitAPI::Util::runFor(0.1_s);
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
    // FIXME: The default LocalStorage path is being used for something, but they shouldn't be. (theses should be false, not true)
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultLocalStoragePath.path]);
#endif

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultIDBPath.path]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:fileIDBPath.path]);

    @autoreleasepool {
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        auto types = adoptNS([[NSSet alloc] initWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil]);
        
        NSURL *appleURL = [NSURL URLWithString:@"https://apple.com"];
        __block NSString *idbPathString = nil;
        done = false;
        [dataStore _originDirectoryForTesting:appleURL topOrigin:fileURL type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
            idbPathString = result;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        NSURL *fileAppleIDBPath = [NSURL fileURLWithPath:idbPathString isDirectory:YES];

        NSURL *webkitURL = [NSURL URLWithString:@"https://webkit.org"];
        done = false;
        [dataStore _originDirectoryForTesting:webkitURL topOrigin:fileURL type:WKWebsiteDataTypeIndexedDBDatabases completionHandler:^(NSString *result) {
            idbPathString = result;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        NSURL *fileWebKitIDBPath = [NSURL fileURLWithPath:idbPathString isDirectory:YES];

        // Subframe of different origins may also create IndexedDB files.
        RetainPtr<NSURL> url1 = [[NSBundle mainBundle] URLForResource:@"IndexedDB" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
        RetainPtr<NSURL> url2 = [[NSBundle mainBundle] URLForResource:@"IndexedDB" withExtension:@"sqlite3-shm" subdirectory:@"TestWebKitAPI.resources"];
        RetainPtr<NSURL> url3 = [[NSBundle mainBundle] URLForResource:@"IndexedDB" withExtension:@"sqlite3-wal" subdirectory:@"TestWebKitAPI.resources"];

        [[NSFileManager defaultManager] createDirectoryAtURL:fileAppleIDBPath withIntermediateDirectories:YES attributes:nil error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:url1.get() toURL:[fileAppleIDBPath URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:url2.get() toURL:[fileAppleIDBPath URLByAppendingPathComponent:@"IndexedDB.sqlite3-shm"] error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:url3.get() toURL:[fileAppleIDBPath URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"] error:nil];

        [[NSFileManager defaultManager] createDirectoryAtURL:fileWebKitIDBPath withIntermediateDirectories:YES attributes:nil error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:url1.get() toURL:[fileWebKitIDBPath URLByAppendingPathComponent:@"IndexedDB.sqlite3"] error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:url2.get() toURL:[fileWebKitIDBPath URLByAppendingPathComponent:@"IndexedDB.sqlite3-shm"] error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:url3.get() toURL:[fileWebKitIDBPath URLByAppendingPathComponent:@"IndexedDB.sqlite3-wal"] error:nil];

        [dataStore fetchDataRecordsOfTypes:types.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
            EXPECT_EQ([records count], (unsigned long)3);
            for (id record in records) {
                if ([[record displayName] isEqual: @"apple.com"]) {
                    [dataStore removeDataOfTypes:types.get() forDataRecords:@[record] completionHandler:^() {
                        receivedScriptMessage = true;
                        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fileAppleIDBPath.path]);
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

        EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fileWebKitIDBPath.path]);

        // Now, with brand new WKWebsiteDataStores pointing at the same custom cookie storage location,
        // in newly fired up NetworkProcesses, verify that the fetch and delete APIs work as expected.
        [dataStore _terminateNetworkProcess];
    }

    auto newCustomDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [newCustomDataStore fetchDataRecordsOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeCookies, nil] completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_GT([records count], (unsigned long)0);
        receivedScriptMessage = true;
    }];
    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    [newCustomDataStore removeDataOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeCookies, nil] modifiedSince:[NSDate distantPast] completionHandler:^ {
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
        ASSERT_EQ(0U, [allProcessPools count]);
        while (![dataStore _networkProcessIdentifier])
            TestWebKitAPI::Util::runFor(0.01_s);
        kill([dataStore _networkProcessIdentifier], SIGKILL);
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
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:defaultDirectory.path]);

    auto webView1 = adoptNS([[TestWKWebView alloc] init]);
    [webView1 synchronouslyLoadHTMLString:@"start auxiliary processes" baseURL:nil];

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    // We always create the path, even if HTTP/3 is turned off.
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultDirectory.path]);

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]).get()]).get()];
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 synchronouslyLoadHTMLString:@"start auxiliary processes" baseURL:nil];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:defaultDirectory.path]);
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
    configuration.get().websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]).get();
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
    webViewConfiguration.get().websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    TestWebKitAPI::Util::spinRunLoop(1);

    [webViewConfiguration.get().websiteDataStore _terminateNetworkProcess];

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
}

TEST(WebKit, WebsiteDataStoreConfigurationPathNull)
{
    EXPECT_TRUE([adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]) _indexedDBDatabaseDirectory]);
    EXPECT_FALSE([adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]) _indexedDBDatabaseDirectory]);
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
    [webView stringByEvaluatingJavaScript:@"localStorage.setItem('testkey', 'testvalue')"];

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
    // Ensure LocalStorage data is not moved.
    EXPECT_WK_STREQ("<null>", [webView stringByEvaluatingJavaScript:@"localStorage.getItem('testkey')"]);

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
    auto webView = adoptNS([[TestWKWebView alloc] init]);
    [webView synchronouslyLoadHTMLString:@"<script>localStorage.setItem('testkey', 'testvalue')</script>" baseURL:[NSURL URLWithString:@"https://example.com/"]];
    
    __block bool done = false;
    NSURL *exampleURL = [NSURL URLWithString:@"https://example.com/"];
    NSURL *webKitURL = [NSURL URLWithString:@"https://webkit.org/"];
    WKWebsiteDataStore *dataStore = [webView configuration].websiteDataStore;
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
    HTTPServer server([] (Connection connection) {
        connection.receiveHTTPRequest([=] (Vector<char>&&) {
            constexpr auto response =
            "HTTP/1.1 200 OK\r\n"
            "Cache-Control: max-age=1000000\r\n"
            "Content-Length: 6\r\n\r\n"
            "Hello!"_s;
            connection.send(response);
        });
    });
    
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"CustomPathsTest"] isDirectory:YES];
    
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [websiteDataStoreConfiguration setNetworkCacheDirectory:tempDir];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];

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
        { "/index.html"_s, { "<html manifest='test.appcache'>"_s } },
        { "/test.appcache"_s, { "CACHE MANIFEST\nindex.html\ntest.mp4\n"_s } },
        { "/test.mp4"_s, { {{ "Content-Type"_s, "video/test"_s }}, "test!"_s }},
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
    [webViewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView.get().configuration.preferences _setOfflineApplicationCacheIsEnabled:YES];
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
        { "/"_s, { {{ "alt-svc"_s, "h3-24=\":443\"; ma=3600; persist=1"_s }}, "<html>test content</html>"_s } }
    }, HTTPServer::Protocol::Https);

    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"AlternativeServiceTest"] isDirectory:YES];
    NSString *path = [tempDir URLByAppendingPathComponent:@"AlternativeService.sqlite"].path;
    NSError *error = nil;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtPath:path error:&error];

    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [dataStoreConfiguration setAlternativeServicesStorageDirectory:tempDir];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    __block bool done = false;
    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeAlternativeServices] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 0u);
        done = true;
    }];
    Util::run(&done);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setWebsiteDataStore:dataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };
    
    [webView loadRequest:server.request()];

    done = false;
    checkUntilEntryFound(dataStore.get(), ^(NSArray<WKWebsiteDataRecord *> *records) {
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

static void respondToRangeRequests(const TestWebKitAPI::Connection& connection, const RetainPtr<NSData>& data)
{
    connection.receiveHTTPRequest([=] (Vector<char>&& bytes) {
        StringView request(reinterpret_cast<const LChar*>(bytes.data()), bytes.size());
        auto rangeBytes = "Range: bytes="_s;
        auto begin = request.find(StringView(rangeBytes), 0);
        ASSERT(begin != notFound);
        auto dash = request.find('-', begin);
        ASSERT(dash != notFound);
        auto end = request.find('\r', dash);
        ASSERT(end != notFound);

        auto rangeBegin = parseInteger<uint64_t>(request.substring(begin + rangeBytes.length(), dash - begin - rangeBytes.length())).value();
        auto rangeEnd = parseInteger<uint64_t>(request.substring(dash + 1, end - dash - 1)).value();

        NSString *responseHeaderString = [NSString stringWithFormat:
            @"HTTP/1.1 206 Partial Content\r\n"
            "Content-Range: bytes %llu-%llu/%llu\r\n"
            "Content-Length: %llu\r\n\r\n",
            rangeBegin, rangeEnd, static_cast<uint64_t>(data.get().length), rangeEnd - rangeBegin];
        NSData *responseHeader = [responseHeaderString dataUsingEncoding:NSUTF8StringEncoding];
        NSData *responseBody = [data subdataWithRange:NSMakeRange(rangeBegin, rangeEnd - rangeBegin)];
        Vector<uint8_t> response { static_cast<const uint8_t*>(responseHeader.bytes), responseHeader.length };
        response.append(static_cast<const uint8_t*>(responseBody.bytes), responseBody.length);
        connection.send(WTFMove(response), [=] {
            respondToRangeRequests(connection, data);
        });
    });
}

TEST(WebKit, MediaCache)
{
    JSC::Config::configureForTesting();

    using namespace TestWebKitAPI;
    RetainPtr<NSData> data = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"]];

    HTTPServer server([&] (Connection connection) {
        connection.receiveHTTPRequest([=] (Vector<char>&&) {
            constexpr auto firstResponse =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 55\r\n\r\n"
            "<video><source src='test.mp4' type='video/mp4'></video>"_s;
            connection.send(firstResponse, [=] {
                respondToRangeRequests(connection, data);
            });
        });
    });

    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"CustomPathsTest"] isDirectory:YES];
    NSString *path = tempDir.path;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    EXPECT_FALSE([fileManager fileExistsAtPath:path]);

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [websiteDataStoreConfiguration setMediaCacheDirectory:tempDir];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

    NSError *error = nil;
    while (![fileManager contentsOfDirectoryAtPath:path error:&error].count)
        Util::spinRunLoop();
    EXPECT_FALSE(error);

    [[webView configuration].websiteDataStore _terminateNetworkProcess];

    [fileManager removeItemAtPath:path error:&error];
    EXPECT_FALSE(error);
}

TEST(WebKit, MigrateLocalStorageDataToGeneralStorageDirectory)
{
    NSURL *localStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *localStorageFile = [localStorageDirectory URLByAppendingPathComponent:@"https_webkit.org_0.localstorage"];
    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *newLocalStorageFile = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/LocalStorage/localstorage.sqlite3"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:localStorageDirectory error:nil];
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:generalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];

    NSString *htmlString = @"<script> \
        result = localStorage.getItem('testkey'); \
        if (!result) \
            result = '[null]'; \
        window.webkit.messageHandlers.testHandler.postMessage(result); \
        </script>";
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._webStorageDirectory = localStorageDirectory;
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = true;
    auto messageHandler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);

    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        receivedScriptMessage = false;
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        EXPECT_WK_STREQ("[null]", getNextMessage().body);
        [webView stringByEvaluatingJavaScript:@"localStorage.setItem('testkey', 'testvalue')"];

        // Ensure item is stored by getting it in another WKWebView.
        auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        receivedScriptMessage = false;
        [secondWebView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        EXPECT_WK_STREQ("testvalue", getNextMessage().body);
        EXPECT_TRUE([fileManager fileExistsAtPath:localStorageFile.path]);
        EXPECT_FALSE([fileManager fileExistsAtPath:newLocalStorageFile.path]);
    }

    // Create a new WebsiteDataStore that performs migration.
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    auto thirdWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    receivedScriptMessage = false;
    [thirdWebView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ("testvalue", getNextMessage().body);
    EXPECT_FALSE([fileManager fileExistsAtPath:localStorageFile.path]);
    EXPECT_TRUE([fileManager fileExistsAtPath:newLocalStorageFile.path]);
}

TEST(WebKit, MigrateIndexedDBDataToGeneralStorageDirectory)
{
    NSURL *indexedDBDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *indexedDBOriginDirectory = [indexedDBDirectory URLByAppendingPathComponent:@"v1/https_webkit.org_0"];
    static constexpr auto indexedDBDatabaseName = "TestDatabase"_s;
    NSString *hashedIndexedDBDatabaseName = WebCore::SQLiteFileSystem::computeHashForFileName(indexedDBDatabaseName);
    NSURL *indexedDBDatabaseDirectory = [indexedDBOriginDirectory URLByAppendingPathComponent:hashedIndexedDBDatabaseName];
    NSURL *indexedDBFile = [indexedDBDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];
    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *newIndexedDBOriginDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/IndexedDB/"];
    NSURL *newIndexedDBDirectory = [newIndexedDBOriginDirectory URLByAppendingPathComponent:hashedIndexedDBDatabaseName];
    NSURL *newIndexedDBFile = [newIndexedDBDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:indexedDBDirectory error:nil];
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:generalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];

    NSString *htmlString = @"<script> \
        var request = window.indexedDB.open('TestDatabase'); \
        var upgradeneededReceived = false; \
        request.onupgradeneeded = function(event) { \
            var db = event.target.result; \
            var os = db.createObjectStore('TestObjectStore'); \
            os.put('value', 'key'); \
            window.webkit.messageHandlers.testHandler.postMessage('database is created'); \
            upgradeneededReceived = true; \
        }; \
        request.onsuccess = function(event) { \
            if (upgradeneededReceived) \
                return; \
            db = event.target.result; \
            db.transaction('TestObjectStore').objectStore('TestObjectStore').get('key').onsuccess = function(event) { \
                var result = event.target.result ? event.target.result : '[null]'; \
                window.webkit.messageHandlers.testHandler.postMessage(result); \
            }; \
        }; \
        </script>";
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = indexedDBDirectory;
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = true;
    auto messageHandler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);

    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        receivedScriptMessage = false;
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        EXPECT_WK_STREQ("database is created", getNextMessage().body);

        // Ensure item is stored by getting it in another WKWebView.
        auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        receivedScriptMessage = false;
        [secondWebView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        EXPECT_WK_STREQ("value", getNextMessage().body);
        EXPECT_TRUE([fileManager fileExistsAtPath:indexedDBFile.path]);
    }

    // Create a new WebsiteDataStore that performs migration.
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    auto thirdWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    receivedScriptMessage = false;
    [thirdWebView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ("value", getNextMessage().body);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:indexedDBFile.path]);
    EXPECT_TRUE([fileManager fileExistsAtPath:newIndexedDBFile.path]);
}

TEST(WebKit, FetchDataAfterEnablingGeneralStorageDirectory)
{
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceLocalStorageFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory-localstorage" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceIndexedDBFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory-indexeddb" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceOriginFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"origin" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *customLocalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *appleCustomLocalStorageFile = [customLocalStorageDirectory URLByAppendingPathComponent:@"https_apple.com_0.localstorage"];
    NSURL *customIndexedDBStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *appleCustomIndexedDBDatabaseDirectory = [customIndexedDBStorageDirectory URLByAppendingPathComponent:@"v1/https_apple.com_0/620AD548000B0A49C02D2E8D75C32E0F85697B54DF86146ECAE360DE104AB3F9"];
    NSURL *appleCustomIndexedDBDatabaseFile = [appleCustomIndexedDBDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];

    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *webkitGeneralStorageDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs"];
    NSURL *webkitGeneralLocalStorageDirectory = [webkitGeneralStorageDirectory URLByAppendingPathComponent:@"LocalStorage"];
    NSURL *webKitGeneralLocalStorageFile = [webkitGeneralLocalStorageDirectory URLByAppendingPathComponent:@"localstorage.sqlite3"];
    NSURL *appleGeneralStorageDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"xKK8XQeHebxhSzMyotOOtNKp2jWZ4l_CEN4WggYYXbI/xKK8XQeHebxhSzMyotOOtNKp2jWZ4l_CEN4WggYYXbI"];
    NSURL *appleGeneralOriginFile = [appleGeneralStorageDirectory URLByAppendingPathComponent:@"origin"];
    NSURL *appleGeneralLocalStorageFile = [appleGeneralStorageDirectory URLByAppendingPathComponent:@"LocalStorage/localstorage.sqlite3"];
    NSURL *appleGeneralIndexedDBDatabaseFile = [appleGeneralStorageDirectory URLByAppendingPathComponent:@"IndexedDB/620AD548000B0A49C02D2E8D75C32E0F85697B54DF86146ECAE360DE104AB3F9/IndexedDB.sqlite3"];

    // Copy apple.com files to custom path.
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:customLocalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:customLocalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceLocalStorageFile toURL:appleCustomLocalStorageFile error:nil];
    [fileManager createDirectoryAtURL:appleCustomIndexedDBDatabaseDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceIndexedDBFile toURL:appleCustomIndexedDBDatabaseFile error:nil];

    // Copy webkit.org files to new path.
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:webkitGeneralLocalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];
    [fileManager copyItemAtURL:resourceOriginFile toURL:[webkitGeneralStorageDirectory URLByAppendingPathComponent:@"origin"] error:nil];
    [fileManager copyItemAtURL:resourceLocalStorageFile toURL:webKitGeneralLocalStorageFile error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._webStorageDirectory = customLocalStorageDirectory;
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = customIndexedDBStorageDirectory;
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    // Ensure data is fetched from both custom path and new path.
    auto dataTypes = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, nil];
    auto sortFunction = ^(WKWebsiteDataRecord *record1, WKWebsiteDataRecord *record2) {
        return [record1.displayName compare:record2.displayName];
    };
    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:dataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 2u);
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        auto appleRecord = [sortedRecords objectAtIndex:0];
        EXPECT_WK_STREQ(@"apple.com", [appleRecord displayName]);
        EXPECT_TRUE([[appleRecord dataTypes] isEqualToSet:dataTypes]);
        auto webkitRecord = [sortedRecords objectAtIndex:1];
        EXPECT_WK_STREQ(@"webkit.org", [webkitRecord displayName]);
        EXPECT_TRUE([[webkitRecord dataTypes] isEqualToSet:[NSSet setWithObject:WKWebsiteDataTypeLocalStorage]]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Ensure apple.com data is moved to new path after fetch.
    EXPECT_FALSE([fileManager fileExistsAtPath:appleCustomLocalStorageFile.path]);
    EXPECT_FALSE([fileManager fileExistsAtPath:appleCustomIndexedDBDatabaseFile.path]);
    EXPECT_TRUE([fileManager fileExistsAtPath:appleGeneralOriginFile.path]);
    EXPECT_TRUE([fileManager fileExistsAtPath:appleGeneralLocalStorageFile.path]);
    EXPECT_TRUE([fileManager fileExistsAtPath:appleGeneralIndexedDBDatabaseFile.path]);

    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:dataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 2u);
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        EXPECT_WK_STREQ(@"apple.com", [[sortedRecords objectAtIndex:0] displayName]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, DeleteDataAfterEnablingGeneralStorageDirectory)
{
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceLocalStorageFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory-localstorage" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceIndexedDBFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory-indexeddb" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceOriginFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"origin" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *customLocalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/LocalStorage/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *appleCustomLocalStorageFile = [customLocalStorageDirectory URLByAppendingPathComponent:@"https_apple.com_0.localstorage"];
    NSURL *customIndexedDBStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *appleCustomIndexedDBDatabaseDirectory = [customIndexedDBStorageDirectory URLByAppendingPathComponent:@"v1/https_apple.com_0/620AD548000B0A49C02D2E8D75C32E0F85697B54DF86146ECAE360DE104AB3F9"];
    NSURL *appleCustomIndexedDBDatabaseFile = [appleCustomIndexedDBDatabaseDirectory URLByAppendingPathComponent:@"IndexedDB.sqlite3"];

    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *webkitGeneralStorageDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs"];
    NSURL *webkitGeneralLocalStorageDirectory = [webkitGeneralStorageDirectory URLByAppendingPathComponent:@"LocalStorage"];
    NSURL *webKitGeneralLocalStorageFile = [webkitGeneralLocalStorageDirectory URLByAppendingPathComponent:@"localstorage.sqlite3"];
    NSURL *appleGeneralLocalStorageFile = [generalStorageDirectory URLByAppendingPathComponent:@"xKK8XQeHebxhSzMyotOOtNKp2jWZ4l_CEN4WggYYXbI/xKK8XQeHebxhSzMyotOOtNKp2jWZ4l_CEN4WggYYXbI/LocalStorage/localstorage.sqlite3"];
    NSURL *appleGeneralIndexedDBDatabaseFile = [generalStorageDirectory URLByAppendingPathComponent:@"xKK8XQeHebxhSzMyotOOtNKp2jWZ4l_CEN4WggYYXbI/xKK8XQeHebxhSzMyotOOtNKp2jWZ4l_CEN4WggYYXbI/IndexedDB/620AD548000B0A49C02D2E8D75C32E0F85697B54DF86146ECAE360DE104AB3F9/IndexedDB.sqlite3"];

    // Copy apple.com files to custom path.
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:customLocalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:customLocalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceLocalStorageFile toURL:appleCustomLocalStorageFile error:nil];
    [fileManager createDirectoryAtURL:appleCustomIndexedDBDatabaseDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceIndexedDBFile toURL:appleCustomIndexedDBDatabaseFile error:nil];

    // Copy webkit.org files to new path.
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:webkitGeneralLocalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];
    [fileManager copyItemAtURL:resourceOriginFile toURL:[webkitGeneralStorageDirectory URLByAppendingPathComponent:@"origin"] error:nil];
    [fileManager copyItemAtURL:resourceLocalStorageFile toURL:webKitGeneralLocalStorageFile error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._webStorageDirectory = customLocalStorageDirectory;
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = customIndexedDBStorageDirectory;
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    auto dataTypes = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, nil];
    done = false;
    [websiteDataStore removeDataOfTypes:dataTypes modifiedSince:[NSDate distantPast] completionHandler:^ {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Ensure data is deleted from both custom path and new path.
    EXPECT_FALSE([fileManager fileExistsAtPath:appleCustomLocalStorageFile.path]);
    EXPECT_FALSE([fileManager fileExistsAtPath:appleCustomIndexedDBDatabaseFile.path]);
    EXPECT_FALSE([fileManager fileExistsAtPath:webKitGeneralLocalStorageFile.path]);

    // Ensure apple.com data is not just moved but actually deleted.
    EXPECT_FALSE([fileManager fileExistsAtPath:appleGeneralLocalStorageFile.path]);
    EXPECT_FALSE([fileManager fileExistsAtPath:appleGeneralIndexedDBDatabaseFile.path]);

    // Ensure data records do not exist after deletion.
    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:dataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 0u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, CreateOriginFileWhenNeeded)
{
    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:generalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];
    NSURL *originFile = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/origin"];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [fileManager removeItemAtURL:websiteDataStoreConfiguration.get()._webStorageDirectory error:nil];
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get()];
    auto handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *htmlString = @"<script> \
        localStorage.getItem('testkey'); \
        window.webkit.messageHandlers.testHandler.postMessage('done'); \
        </script>";
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    // No origin file for read.
    EXPECT_FALSE([fileManager fileExistsAtPath:originFile.path]);

    [webView stringByEvaluatingJavaScript:@"localStorage.setItem('testkey', 'testvalue')"];
    // Origin file is created for write.
    while (![fileManager fileExistsAtPath:originFile.path])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(WKWebsiteDataStoreConfiguration, SameCustomPathForDifferentTypes)
{
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    auto dataTypes = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, WKWebsiteDataTypeCookies, nil];

    NSURL *sharedDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/SamePathForDifferentTypes" stringByExpandingTildeInPath] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:sharedDirectory error:nil];
    websiteDataStoreConfiguration.get()._webSQLDatabaseDirectory = sharedDirectory;
    websiteDataStoreConfiguration.get()._webStorageDirectory = sharedDirectory;
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = sharedDirectory;
    websiteDataStoreConfiguration.get()._cookieStorageFile = [sharedDirectory URLByAppendingPathComponent:@"Cookies.binarycookies" isDirectory:NO];
    websiteDataStoreConfiguration.get().generalStorageDirectory = [sharedDirectory URLByAppendingPathComponent:@"Default" isDirectory:YES];

    @autoreleasepool {
        auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setWebsiteDataStore:websiteDataStore.get()];
        auto messageHandler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"WebsiteDataStoreCustomPaths" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        EXPECT_STREQ([getNextMessage().body UTF8String], "localstorage written");
        EXPECT_STREQ([getNextMessage().body UTF8String], "cookie written");
        EXPECT_STREQ([getNextMessage().body UTF8String], "Success opening indexed database");

        // Ensure cookies are written to file.
        done = false;
        [websiteDataStore.get().httpCookieStore _flushCookiesToDiskWithCompletionHandler:^{
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        done = false;
        [websiteDataStore fetchDataRecordsOfTypes:dataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
            EXPECT_EQ([records count], 1u);
            EXPECT_TRUE([[[records firstObject] dataTypes] isEqualToSet:dataTypes]);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    }

    auto newWebsiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    done = false;
    [newWebsiteDataStore fetchDataRecordsOfTypes:dataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], 1u);
        EXPECT_TRUE([[[records firstObject] dataTypes] isEqualToSet:dataTypes]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, DeleteEmptyOriginDirectoryWhenFetchData)
{
    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *topOriginDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs"];
    NSURL *originDirectory = [topOriginDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs"];
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceOriginFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"origin" subdirectory:@"TestWebKitAPI.resources"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:generalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];
    [fileManager createDirectoryAtURL:originDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceOriginFile toURL:[originDirectory URLByAppendingPathComponent:@"origin"] error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [fileManager removeItemAtURL:websiteDataStoreConfiguration.get()._webStorageDirectory error:nil];
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    done = false;
    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, nil] completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([fileManager fileExistsAtPath:originDirectory.path]);
    EXPECT_FALSE([fileManager fileExistsAtPath:topOriginDirectory.path]);
}

TEST(WebKit, DeleteEmptyOriginDirectoryWhenOriginIsGone)
{
    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *topOriginDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs"];
    NSURL *originDirectory = [topOriginDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs"];
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceOriginFile = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"origin" subdirectory:@"TestWebKitAPI.resources"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:generalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];
    [fileManager createDirectoryAtURL:originDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    // Baked origin file webkit.org.
    [fileManager copyItemAtURL:resourceOriginFile toURL:[originDirectory URLByAppendingPathComponent:@"origin"] error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [fileManager removeItemAtURL:websiteDataStoreConfiguration.get()._webStorageDirectory error:nil];
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
    auto handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *htmlString = @"<script> \
        localStorage.getItem('testkey'); \
        window.webkit.messageHandlers.testHandler.postMessage('done'); \
        </script>";
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    auto pid = [webView _webProcessIdentifier];
    EXPECT_NE(pid, 0);

    // Ensure webkit.org is gone.
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://apple.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    kill(pid, SIGKILL);
    while ([fileManager fileExistsAtPath:topOriginDirectory.path])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_FALSE([fileManager fileExistsAtPath:topOriginDirectory.path]);
}

#if PLATFORM(IOS_FAMILY)

TEST(WebKit, OriginDirectoryExcludedFromBackup)
{
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *generalStorageDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/Default" stringByExpandingTildeInPath] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:generalStorageDirectory error:nil];
    [fileManager createDirectoryAtURL:generalStorageDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:[generalStorageDirectory URLByAppendingPathComponent:@"salt"] error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get().generalStorageDirectory = generalStorageDirectory;
    websiteDataStoreConfiguration.get().shouldUseCustomStoragePaths = false;
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    NSString *htmlString = @"<script> \
        localStorage.setItem('local', 'storage'); \
        sessionStorage.setItem('session', 'storage'); \
        window.webkit.messageHandlers.testHandler.postMessage('done'); \
        </script>";
    auto handler = adoptNS([[WebsiteDataStoreCustomPathsMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    NSURL *originDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/"];
    EXPECT_TRUE([fileManager fileExistsAtPath:originDirectory.path]);
    NSNumber *isDirectoryExcluded = nil;
    EXPECT_TRUE([originDirectory getResourceValue:&isDirectoryExcluded forKey:NSURLIsExcludedFromBackupKey error:nil]);
    EXPECT_TRUE(isDirectoryExcluded.boolValue);

    // Set exclusion period to 0 so excluded attribute is updated on next visit.
    done = false;
    [websiteDataStore _setBackupExclusionPeriodForTesting:0.0 completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    receivedScriptMessage = false;
    [webView2 loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    // Create new url that has updated value.
    originDirectory = [generalStorageDirectory URLByAppendingPathComponent:@"YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/YUn_wgR51VLVo9lc5xiivAzZ8TMmojoa0IbW323qibs/"];
    EXPECT_TRUE([originDirectory getResourceValue:&isDirectoryExcluded forKey:NSURLIsExcludedFromBackupKey error:nil]);
    EXPECT_FALSE(isDirectoryExcluded.boolValue);
}

#endif

TEST(WebKit, RemoveStaleWebSQLData)
{
    NSURL *resourceWebSQLDatabaseTrackerFile = [[NSBundle mainBundle] URLForResource:@"websql-database-tracker" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceWebSQLDatabaseFile = [[NSBundle mainBundle] URLForResource:@"websql-database" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *resourceSalt = [[NSBundle mainBundle] URLForResource:@"general-storage-directory" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *customWebSQLDirectory = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebSQL" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *databaseTrackerFile = [customWebSQLDirectory URLByAppendingPathComponent:@"Databases.db"];
    NSURL *webkitSQLDirectory = [customWebSQLDirectory URLByAppendingPathComponent:@"http_webkit.org_0"];
    NSURL *databaseFile = [webkitSQLDirectory URLByAppendingPathComponent:@"f3013c11-b9f1-4534-a9d6-f3426f777eb3.db"];
    NSURL *salt = [customWebSQLDirectory URLByAppendingPathComponent:@"salt"];

    /* Directory structure.
    - CustomWebSQL
        - salt
        - Databases.db
        - http_webkit.org_0
            - f3013c11-b9f1-4534-a9d6-f3426f777eb3.db
    */
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:customWebSQLDirectory error:nil];
    [fileManager createDirectoryAtURL:customWebSQLDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceWebSQLDatabaseTrackerFile toURL:databaseTrackerFile error:nil];
    [fileManager createDirectoryAtURL:webkitSQLDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    [fileManager copyItemAtURL:resourceWebSQLDatabaseFile toURL:databaseFile error:nil];
    [fileManager copyItemAtURL:resourceSalt toURL:salt error:nil];

    // Ensure WebSQL files are deleted.
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._webSQLDatabaseDirectory = customWebSQLDirectory;
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray*) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([fileManager fileExistsAtPath:databaseTrackerFile.path]);
    EXPECT_FALSE([fileManager fileExistsAtPath:databaseFile.path]);
    EXPECT_FALSE([fileManager fileExistsAtPath:webkitSQLDirectory.path]);
    EXPECT_TRUE([fileManager fileExistsAtPath:salt.path]);

    // Ensure WebSQL directory is deleted.
    [fileManager removeItemAtURL:salt error:nil];
    websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray*) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([fileManager fileExistsAtPath:customWebSQLDirectory.path]);
}

TEST(WKWebsiteDataStoreConfiguration, InitWithIdentifier)
{
    NSString *htmlString = @"<script> \
        document.cookie = \"testkey=value; expires=Mon, 01 Jan 2035 00:00:00 GMT\"; \
    </script>";
    NSString *uuidString = @"68753a44-4d6f-1226-9c60-0050e4c00067";
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:uuidString]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    NSURL *cookieStorageFile = websiteDataStoreConfiguration.get()._cookieStorageFile;
    EXPECT_TRUE([cookieStorageFile.path containsString:uuidString]);
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:cookieStorageFile error:nil];
    EXPECT_FALSE([fileManager fileExistsAtPath:cookieStorageFile.path]);

    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    while (![fileManager fileExistsAtPath:cookieStorageFile.path])
        TestWebKitAPI::Util::spinRunLoop();

    auto types = [NSSet setWithObject:WKWebsiteDataTypeCookies];
    [websiteDataStore.get() fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], 1u);
        EXPECT_WK_STREQ([[records firstObject] displayName], @"webkit.org");
    }];
}

TEST(WKWebsiteDataStoreConfiguration, InitWithInvalidIdentifier)
{
    bool hasException = false;
    @try {
        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:[[NSUUID alloc] initWithUUIDString:@"1234"]]);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}

TEST(WKWebsiteDataStoreConfiguration, SetPathOnConfigurationWithIdentifier)
{
    bool hasException = false;
    @try {
        auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753A44-4D6F-1226-9C60-0050E4C00067"]);
        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
        NSURL *cookieStorageFile = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/CookieStorage/Cookies.binarycookies" stringByExpandingTildeInPath] isDirectory:NO];
        websiteDataStoreConfiguration.get()._cookieStorageFile = cookieStorageFile;
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSGenericException, exception.name);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}
