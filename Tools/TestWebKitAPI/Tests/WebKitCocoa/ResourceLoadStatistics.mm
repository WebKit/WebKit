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
#import "TCPServer.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

static bool finishedNavigation = false;

@interface _WKResourceLoadStatisticsFirstParty : NSObject
@property (nonatomic, readonly) NSString *firstPartyDomain;
@property (nonatomic, readonly) BOOL thirdPartyStorageAccessGranted;
@property (nonatomic, readonly) NSTimeInterval timeLastUpdated;
@end

@interface _WKResourceLoadStatisticsThirdParty : NSObject
@property (nonatomic, readonly) NSString *thirdPartyDomain;
@property (nonatomic, readonly) NSArray<_WKResourceLoadStatisticsFirstParty *> *underFirstParties;
@end

@interface DisableITPDuringNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DisableITPDuringNavigationDelegate

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    // Disable ITP so that the WebResourceLoadStatisticsStore gets destroyed before its has a chance to receive the notification from the page.
    [[WKWebsiteDataStore defaultDataStore] _setResourceLoadStatisticsEnabled:NO];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedNavigation = true;
}

@end

bool isITPDatabaseEnabled()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto defaultDatabaseEnabled = [configuration preferences]._isITPDatabaseEnabled;

    NSNumber *databaseEnabledValue = [defaults objectForKey:@"InternalDebugIsITPDatabaseEnabled"];
    if (databaseEnabledValue)
        return databaseEnabledValue.boolValue;

    return defaultDatabaseEnabled;
}

static void ensureITPFileIsCreated()
{
    static bool doneFlag;
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [dataStore _setUseITPDatabase:true completionHandler: ^(void) {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    [dataStore _setResourceLoadStatisticsEnabled:NO];
}

TEST(ResourceLoadStatistics, GrandfatherCallback)
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *fileURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"full_browsing_session_resourceLog.plist"];
    [[NSFileManager defaultManager] removeItemAtURL:fileURL error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:statisticsDirectoryURL error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:statisticsDirectoryURL.path]);

    static bool doneFlag;
    static bool grandfatheredFlag;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        // Only if the database store is not default-enabled do we need to check that this first message is correct.
        if (!isITPDatabaseEnabled())
            ASSERT_TRUE([message isEqualToString:@"Grandfathered"]);
        grandfatheredFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    if (isITPDatabaseEnabled()) {
        TestWebKitAPI::Util::run(&grandfatheredFlag);

        // Since the database store is enabled, we need to disable it to test this functionality with the plist.
        // We already have a NetworkProcess up and running with ITP enabled. We now make another call initializing
        // the memory store and testing grandfathering.
        grandfatheredFlag = false;
        [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
            ASSERT_TRUE([message isEqualToString:@"Grandfathered"]);
            grandfatheredFlag = true;
        }];

        [dataStore _setUseITPDatabase:false completionHandler: ^(void) {
            doneFlag = true;
        }];

        TestWebKitAPI::Util::run(&doneFlag);
    }

    TestWebKitAPI::Util::run(&grandfatheredFlag);

    // Spin the runloop until the resource load statistics file has written to disk.
    // If the test enters a spin loop here, it has failed.
    while (true) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:fileURL.path])
            break;
        TestWebKitAPI::Util::spinRunLoop(1);
    }

    grandfatheredFlag = false;
    doneFlag = false;
    [dataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate]  modifiedSince:[NSDate distantPast] completionHandler:^ {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    TestWebKitAPI::Util::spinRunLoop(10);

    // The website data store remove should have completed, but since we removed all of the data types that are monitored by resource load statistics,
    // no grandfathering call should have been made.
    EXPECT_FALSE(grandfatheredFlag);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:fileURL.path]);

    doneFlag = false;
    [dataStore removeDataOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeCookies, _WKWebsiteDataTypeResourceLoadStatistics, nil] modifiedSince:[NSDate distantPast] completionHandler:^ {
        doneFlag = true;
    }];

    // Since we did not remove every data type covered by resource load statistics, we do expect that grandfathering took place again.
    // If the test hangs waiting on either of these conditions, it has failed.
    TestWebKitAPI::Util::run(&grandfatheredFlag);
    TestWebKitAPI::Util::run(&doneFlag);

    // Spin the runloop until the resource load statistics file has written to disk.
    // If the test enters a spin loop here, it has failed.
    while (true) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:fileURL.path])
            break;
        TestWebKitAPI::Util::spinRunLoop(1);
    }
}

TEST(ResourceLoadStatistics, GrandfatherCallbackDatabase)
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *fileURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"observations.db"];
    [[NSFileManager defaultManager] removeItemAtURL:fileURL error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:statisticsDirectoryURL error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:statisticsDirectoryURL.path]);

    static bool grandfatheredFlag;
    static bool doneFlag;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        // Only if the database store is default-enabled do we need to check that this first message is correct.
        if (isITPDatabaseEnabled())
            ASSERT_TRUE([message isEqualToString:@"Grandfathered"]);
        grandfatheredFlag = true;
    }];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    if (!isITPDatabaseEnabled()) {
        TestWebKitAPI::Util::run(&grandfatheredFlag);

        // Since the database store is not enabled, we need to update the store to use a database instead of a plist for this test.
        // We already have a NetworkProcess up and running with ITP enabled. We now make another call initializing
        // the database store and testing grandfathering.
        grandfatheredFlag = false;
        [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
            ASSERT_TRUE([message isEqualToString:@"Grandfathered"]);
            grandfatheredFlag = true;
        }];

        [dataStore _setUseITPDatabase:true completionHandler: ^(void) {
            doneFlag = true;
        }];

        TestWebKitAPI::Util::run(&doneFlag);
    }

    TestWebKitAPI::Util::run(&grandfatheredFlag);

    // Spin the runloop until the resource load statistics file has written to disk.
    // If the test enters a spin loop here, it has failed.
    while (true) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:fileURL.path])
            break;
        TestWebKitAPI::Util::spinRunLoop(1);
    }

    grandfatheredFlag = false;
    doneFlag = false;
    [dataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate]  modifiedSince:[NSDate distantPast] completionHandler:^ {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    TestWebKitAPI::Util::spinRunLoop(10);

    // The website data store remove should have completed, but since we removed all of the data types that are monitored by resource load statistics,
    // no grandfathering call should have been made. Note that the database file will not be deleted like in the persistent storage case, only cleared.
    EXPECT_FALSE(grandfatheredFlag);

    doneFlag = false;
    [dataStore removeDataOfTypes:[NSSet setWithObjects:WKWebsiteDataTypeCookies, _WKWebsiteDataTypeResourceLoadStatistics, nil] modifiedSince:[NSDate distantPast] completionHandler:^ {
        doneFlag = true;
    }];

    // Since we did not remove every data type covered by resource load statistics, we do expect that grandfathering took place again.
    // If the test hangs waiting on either of these conditions, it has failed.
    TestWebKitAPI::Util::run(&grandfatheredFlag);
    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, ShouldNotGrandfatherOnStartup)
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *targetURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"full_browsing_session_resourceLog.plist"];
    NSURL *testResourceURL = [[NSBundle mainBundle] URLForResource:@"EmptyGrandfatheredResourceLoadStatistics" withExtension:@"plist" subdirectory:@"TestWebKitAPI.resources"];

    [[NSFileManager defaultManager] createDirectoryAtURL:statisticsDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:testResourceURL toURL:targetURL error:nil];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

    static bool callbackFlag;
    static bool doneFlag;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        // Only if the database store is not default-enabled do we need to check that this first message is correct.
        if (!isITPDatabaseEnabled())
            ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        callbackFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    if (isITPDatabaseEnabled()) {
        TestWebKitAPI::Util::run(&callbackFlag);

        // Since the database store is enabled, we need to disable it to test this functionality with the plist.
        // We already have a NetworkProcess up and running with ITP enabled. We now make another call initializing
        // the memory store and testing grandfathering.
        callbackFlag = false;
        [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
            ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
            callbackFlag = true;
        }];

        // Since the ITP Database is enabled, the plist file has been deleted. We need to create it again.
        [[NSFileManager defaultManager] createDirectoryAtURL:statisticsDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:testResourceURL toURL:targetURL error:nil];
        EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

        [dataStore _setUseITPDatabase:false completionHandler: ^(void) {
            doneFlag = true;
        }];

        TestWebKitAPI::Util::run(&doneFlag);
    }

    TestWebKitAPI::Util::run(&callbackFlag);
}

TEST(ResourceLoadStatistics, ShouldNotGrandfatherOnStartupDatabase)
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    ensureITPFileIsCreated();

    static bool callbackFlag;
    static bool doneFlag;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        // Only if the database store is default-enabled do we need to check that this first message is correct.
        if (isITPDatabaseEnabled())
            ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        callbackFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    if (!isITPDatabaseEnabled()) {
        TestWebKitAPI::Util::run(&callbackFlag);

        // Since the database store is not enabled, we need to update the store to use a database instead of a plist for this test.
        // We already have a NetworkProcess up and running with ITP enabled. We now make another call initializing
        // the database store and testing grandfathering.
        callbackFlag = false;
        [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
            ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
            callbackFlag = true;
        }];

        ensureITPFileIsCreated();

        [dataStore _setUseITPDatabase:true completionHandler: ^(void) {
            doneFlag = true;
        }];

        TestWebKitAPI::Util::run(&doneFlag);
    }

    TestWebKitAPI::Util::run(&callbackFlag);
}

TEST(ResourceLoadStatistics, ChildProcessesNotLaunched)
{
    // Ensure the shared process pool exists so the data store operations we're about to do work with it.
    WKProcessPool *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    EXPECT_EQ((size_t)0, [sharedProcessPool _pluginProcessCount]);

    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *targetURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"full_browsing_session_resourceLog.plist"];
    NSURL *testResourceURL = [[NSBundle mainBundle] URLForResource:@"EmptyGrandfatheredResourceLoadStatistics" withExtension:@"plist" subdirectory:@"TestWebKitAPI.resources"];

    [[NSFileManager defaultManager] createDirectoryAtURL:statisticsDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:testResourceURL toURL:targetURL error:nil];
    
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

    static bool callbackFlag;
    static bool doneFlag;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        // Only if the database store is not default-enabled do we need to check that this first message is correct.
        if (!isITPDatabaseEnabled())
            EXPECT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        callbackFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    if (isITPDatabaseEnabled()) {
        TestWebKitAPI::Util::run(&callbackFlag);

        // Since the database store is enabled, we need to disable it to test this functionality with the plist.
        // We already have a NetworkProcess up and running with ITP enabled. We now make another call initializing
        // the memory store and testing grandfathering.
        callbackFlag = false;
        [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
            ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
            callbackFlag = true;
        }];

        // Since the ITP Database is enabled, the plist file has been deleted. We need to create it again.
        [[NSFileManager defaultManager] createDirectoryAtURL:statisticsDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
        [[NSFileManager defaultManager] copyItemAtURL:testResourceURL toURL:targetURL error:nil];
        EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

        [dataStore _setUseITPDatabase:false completionHandler: ^(void) {
            doneFlag = true;
        }];

        TestWebKitAPI::Util::run(&doneFlag);
    }

    TestWebKitAPI::Util::run(&callbackFlag);

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

    webView.clear();

    EXPECT_EQ((size_t)0, [sharedProcessPool _pluginProcessCount]);
}

TEST(ResourceLoadStatistics, ChildProcessesNotLaunchedDatabase)
{
    // Ensure the shared process pool exists so the data store operations we're about to do work with it.
    WKProcessPool *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    EXPECT_EQ((size_t)0, [sharedProcessPool _pluginProcessCount]);

    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *targetURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"observations.db"];

    ensureITPFileIsCreated();

    static bool callbackFlag;
    static bool doneFlag;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        // Only if the database store is default-enabled do we need to check that this first message is correct.
        if (isITPDatabaseEnabled())
            EXPECT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        callbackFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    if (!isITPDatabaseEnabled()) {
        TestWebKitAPI::Util::run(&callbackFlag);

        // Since the database store is not enabled, we need to update the store to use a database instead of a plist for this test.
        // We already have a NetworkProcess up and running with ITP enabled. We now make another call initializing
        // the database store and testing grandfathering.
        callbackFlag = false;
        [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
            ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
            callbackFlag = true;
        }];

        ensureITPFileIsCreated();

        [dataStore _setUseITPDatabase:true completionHandler: ^(void) {
            doneFlag = true;
        }];

        TestWebKitAPI::Util::run(&doneFlag);
    }

    TestWebKitAPI::Util::run(&callbackFlag);

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

    webView.clear();

    EXPECT_EQ((size_t)0, [sharedProcessPool _pluginProcessCount]);
}

TEST(ResourceLoadStatistics, IPCAfterStoreDestruction)
{
    [[WKWebsiteDataStore defaultDataStore] _setResourceLoadStatisticsEnabled:YES];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    // Test page requires window.internals.
#if WK_HAVE_C_SPI
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
#endif

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[DisableITPDuringNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"notify-resourceLoadObserver" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    TestWebKitAPI::Util::run(&finishedNavigation);
}

static void cleanupITPDatabase(WKWebsiteDataStore *dataStore)
{
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    // Make sure 'evil.com' is not in our data set.
    static bool doneFlag;
    static bool dataSyncCompleted;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        if (![message isEqualToString:@"Storage Synced"])
            return;

        dataSyncCompleted = true;
    }];
    [dataStore _clearPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Trigger ITP to process its data to force a sync to persistent storage.
    [dataStore _processStatisticsAndDataRecords: ^(void) {
        doneFlag = true;
    }];
    
    TestWebKitAPI::Util::run(&doneFlag);
    TestWebKitAPI::Util::run(&dataSyncCompleted);
    
    TestWebKitAPI::Util::spinRunLoop(1);

    [dataStore _setResourceLoadStatisticsEnabled:NO];
}

TEST(ResourceLoadStatistics, EnableDisableITP)
{
    // Ensure the shared process pool exists so the data store operations we're about to do work with it.
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    cleanupITPDatabase(dataStore);

    // ITP should be off, no URLs are prevalent.
    static bool doneFlag;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_FALSE(prevalent);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Turn it on
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];
    
    // ITP should be on, but nothing was registered as prevalent yet.
    doneFlag = false;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_FALSE(prevalent);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Teach ITP about a bad origin:
    doneFlag = false;
    [dataStore _setPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    // ITP should be on, and know about 'evil.com'
    doneFlag = false;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_TRUE(prevalent);
        doneFlag = true;
    }];
    
    TestWebKitAPI::Util::run(&doneFlag);

    // Turn it off
    [dataStore _setResourceLoadStatisticsEnabled:NO];

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    // ITP should be off, no URLs are prevalent.
    doneFlag = false;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_FALSE(prevalent);
        doneFlag = true;
    }];
    
    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, RemoveSessionID)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // We load a resource so that the NetworkSession stays alive a little bit longer after the session is removed.

    [webView loadHTMLString:@"<a id='link' href='http://webkit.org' download>Click me!</a>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    static bool doneFlag = false;
    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler: ^(id, NSError*) {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    [configuration.get().websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [configuration.get().websiteDataStore _setResourceLoadStatisticsDebugMode:YES];

    // Trigger ITP tasks.
    [configuration.get().websiteDataStore _scheduleCookieBlockingUpdate: ^(void) { }];
    // Trigger removing of the sessionID.
    TestWebKitAPI::Util::spinRunLoop(2);
    [webView _close];
    webView = nullptr;
    configuration = nullptr;

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView2 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView2 _test_waitForDidFinishNavigation];
}

TEST(ResourceLoadStatistics, NetworkProcessRestart)
{
    // Ensure the shared process pool exists so the data store operations we're about to do work with it.
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    cleanupITPDatabase(dataStore);

    // Turn it on
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    // ITP should be on, but nothing was registered as prevalent yet.
    static bool doneFlag;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_FALSE(prevalent);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Teach ITP about a bad origin:
    doneFlag = false;
    [dataStore _setPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    // ITP should be on, and know about 'evil.com'
    doneFlag = false;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_TRUE(prevalent);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    static bool dataSyncCompleted;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        if (![message isEqualToString:@"Storage Synced"])
            return;
        dataSyncCompleted = true;
    }];

    // Tell ITP to process data so it will sync the new 'bad guy' to persistent storage:
    [dataStore _processStatisticsAndDataRecords: ^(void) {
        doneFlag = true;
     }];

    TestWebKitAPI::Util::run(&doneFlag);
    TestWebKitAPI::Util::run(&dataSyncCompleted);

    TestWebKitAPI::Util::spinRunLoop(1);

    [configuration.get().processPool _terminateNetworkProcess];

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView2 loadHTMLString:@"WebKit Test 2" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView2 _test_waitForDidFinishNavigation];

    // ITP should be on, and know about 'evil.com'
    doneFlag = false;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_TRUE(prevalent);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Turn it off
    [dataStore _setResourceLoadStatisticsEnabled:NO];

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    // ITP should be off, no URLs are prevalent.
    doneFlag = false;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_FALSE(prevalent);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    TestWebKitAPI::Util::spinRunLoop(1);

    [configuration.get().processPool _terminateNetworkProcess];

    auto webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView3 loadHTMLString:@"WebKit Test 3" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView3 _test_waitForDidFinishNavigation];

    TestWebKitAPI::Util::run(&doneFlag);

    // ITP should still be off, and should not know about 'evil.com'
    doneFlag = false;
    [dataStore _getIsPrevalentDomain:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL prevalent) {
        EXPECT_FALSE(prevalent);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

@interface DataTaskIdentifierCollisionDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
- (NSArray<NSString *> *)waitForMessages:(size_t)count;
@end

@implementation DataTaskIdentifierCollisionDelegate {
    RetainPtr<NSMutableArray<NSString *>> _messages;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    [_messages addObject:message];
    completionHandler();
}

- (NSArray<NSString *> *)waitForMessages:(size_t)messageCount
{
    _messages = adoptNS([NSMutableArray arrayWithCapacity:messageCount]);
    while ([_messages count] < messageCount)
        TestWebKitAPI::Util::spinRunLoop();
    return _messages.autorelease();
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * credential))completionHandler
{
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}

@end

#if HAVE(SSL)
TEST(ResourceLoadStatistics, DataTaskIdentifierCollision)
{
    using namespace TestWebKitAPI;

    std::atomic<unsigned> serversConnected { 0 };
    const char* header = "HTTP/1.1 200 OK\r\nContent-Length: 27\r\n\r\n";

    TCPServer httpsServer(TCPServer::Protocol::HTTPSProxy, [&] (SSL* ssl) {
        serversConnected++;
        while (serversConnected < 2)
            usleep(100000);
        TCPServer::read(ssl);
        TCPServer::write(ssl, header, strlen(header));
        const char* body = "<script>alert('1')</script>";
        TCPServer::write(ssl, body, strlen(body));
    });

    TCPServer httpServer([&] (int socket) {
        serversConnected++;
        while (serversConnected < 2)
            usleep(100000);
        TCPServer::read(socket);
        TCPServer::write(socket, header, strlen(header));
        const char* body = "<script>alert('2')</script>";
        TCPServer::write(socket, body, strlen(body));
    });

    _WKWebsiteDataStoreConfiguration *storeConfiguration = [[_WKWebsiteDataStoreConfiguration new] autorelease];
    storeConfiguration.httpsProxy = [NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", httpsServer.port()]];
    storeConfiguration.allowsServerPreconnect = NO;
    WKWebsiteDataStore *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration] autorelease];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:dataStore];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [webView1 synchronouslyLoadHTMLString:@"start network process and initialize data store"];
    auto delegate = adoptNS([DataTaskIdentifierCollisionDelegate new]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [webView1 setUIDelegate:delegate.get()];
    [webView1 setNavigationDelegate:delegate.get()];
    [webView2 setUIDelegate:delegate.get()];
    [webView2 setNavigationDelegate:delegate.get()];
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [dataStore _setResourceLoadStatisticsDebugMode:YES];

    __block bool isolatedSessionDomain = false;
    __block NSURL *prevalentExample = [NSURL URLWithString:@"https://prevalent-example.com/"];
    [dataStore _logUserInteraction:prevalentExample completionHandler:^{
        [dataStore _setPrevalentDomain:prevalentExample completionHandler: ^(void) {
            [dataStore _scheduleCookieBlockingUpdate:^{
                isolatedSessionDomain = true;
            }];
        }];
    }];
    TestWebKitAPI::Util::run(&isolatedSessionDomain);

    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", httpServer.port()]]]];
    [webView2 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://prevalent-example.com/"]]];

    NSArray<NSString *> *messages = [delegate waitForMessages:2];
    auto contains = [] (NSArray<NSString *> *array, NSString *expected) {
        for (NSString *s in array) {
            if ([s isEqualToString:expected])
                return true;
        }
        return false;
    };
    EXPECT_TRUE(contains(messages, @"1"));
    EXPECT_TRUE(contains(messages, @"2"));
}
#endif // HAVE(SSL)

TEST(ResourceLoadStatistics, NoMessagesWhenNotTesting)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKWebsiteDataStore *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:[[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease]] autorelease];
    [configuration setWebsiteDataStore:dataStore];
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    EXPECT_FALSE([WKWebsiteDataStore _defaultDataStoreExists]);
}

TEST(ResourceLoadStatistics, FlushObserverWhenWebPageIsClosedByJavaScript)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    [dataStore _setResourceLoadStatisticsEnabled:YES];

    static bool doneFlag = false;
    [dataStore _clearResourceLoadStatistics:^(void) {
        doneFlag = true;
    }];

    static bool statisticsUpdated = false;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        if (![message isEqualToString:@"Statistics Updated"])
            return;
        statisticsUpdated = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Seed test data in the web process' observer.
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    
    TestWebKitAPI::Util::run(&doneFlag);

    // Check that the third-party is not yet registered.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_FALSE(isRegistered);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    statisticsUpdated = false;
    [webView loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    // Wait for the statistics to be updated in the network process.
    TestWebKitAPI::Util::run(&statisticsUpdated);

    // Check that the third-party is now registered.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, GetResourceLoadStatisticsDataSummary)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView1 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView1 _test_waitForDidFinishNavigation];
    [webView2 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit2.org"]];
    [webView2 _test_waitForDidFinishNavigation];
    [webView3 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit3.org"]];
    [webView3 _test_waitForDidFinishNavigation];

    [dataStore _setResourceLoadStatisticsEnabled:YES];

    static bool doneFlag = false;
    [dataStore _setUseITPDatabase:false completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    [dataStore _clearResourceLoadStatistics:^(void) {
        doneFlag = true;
    }];

    static bool statisticsUpdated = false;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        if (![message isEqualToString:@"Statistics Updated"])
            return;
        statisticsUpdated = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    
    doneFlag = false;
    [dataStore _setThirdPartyCookieBlockingMode:true onlyOnSitesWithoutUserInteraction:false completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Set two third parties to be prevalent, leave evil1.com as non-prevalent to ensure
    // this call returns all third parties.
    doneFlag = false;
    [dataStore _setPrevalentDomain:[NSURL URLWithString:@"http://evil2.com"] completionHandler: ^(void) {
        doneFlag = true;
    }];
    doneFlag = false;
    [dataStore _setPrevalentDomain:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(void) {
        doneFlag = true;
    }];

    // Seed test data in the web process' observer.

    // evil1
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);


    // evil2
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil2.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);


    // evil3
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit3.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    statisticsUpdated = false;
    [webView1 loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView2 loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit2.org"]];
    [webView3 loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit3.org"]];

    // Wait for the statistics to be updated in the network process.
    TestWebKitAPI::Util::run(&statisticsUpdated);

    // Check that the third-party evil1 is now registered as subresource.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);


    // Check that the third-party evil2 is now registered as subresource.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil2.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);


    // Check that the third-party evil3 is now registered as subresource.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit3.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);


    // Collect the ITP data summary which should include all third parties in the
    // order: [evil3.com, evil1.com, evil2.com] sorted by number of first parties
    // it appears under or redirects to
    doneFlag = false;
    [dataStore _getResourceLoadStatisticsDataSummary:^void(NSArray<_WKResourceLoadStatisticsThirdParty *> *thirdPartyData)
    {
        EXPECT_EQ(static_cast<int>([thirdPartyData count]), 3);
        NSEnumerator *thirdPartyDomains = [thirdPartyData objectEnumerator];

        // evil3
        _WKResourceLoadStatisticsThirdParty *evil3ThirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(evil3ThirdParty.thirdPartyDomain, @"evil3.com");

        NSEnumerator *evil3Enumerator = [evil3ThirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *evil3FirstParty1 = [evil3Enumerator nextObject];
        _WKResourceLoadStatisticsFirstParty *evil3FirstParty2 = [evil3Enumerator nextObject];
        _WKResourceLoadStatisticsFirstParty *evil3FirstParty3 = [evil3Enumerator nextObject];

        EXPECT_WK_STREQ(evil3FirstParty1.firstPartyDomain, @"webkit2.org");
        EXPECT_WK_STREQ(evil3FirstParty2.firstPartyDomain, @"webkit3.org");
        EXPECT_WK_STREQ(evil3FirstParty3.firstPartyDomain, @"webkit.org");

        EXPECT_FALSE(evil3FirstParty1.thirdPartyStorageAccessGranted);
        EXPECT_FALSE(evil3FirstParty2.thirdPartyStorageAccessGranted);
        EXPECT_FALSE(evil3FirstParty3.thirdPartyStorageAccessGranted);

        // evil1
        _WKResourceLoadStatisticsThirdParty *evil1ThirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(evil1ThirdParty.thirdPartyDomain, @"evil1.com");

        NSEnumerator *evil1Enumerator = [evil1ThirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *evil1FirstParty1= [evil1Enumerator nextObject];
        _WKResourceLoadStatisticsFirstParty *evil1FirstParty2 = [evil1Enumerator nextObject];

        EXPECT_WK_STREQ(evil1FirstParty1.firstPartyDomain, @"webkit2.org");
        EXPECT_WK_STREQ(evil1FirstParty2.firstPartyDomain, @"webkit.org");

        EXPECT_FALSE(evil1FirstParty1.thirdPartyStorageAccessGranted);
        EXPECT_FALSE(evil1FirstParty2.thirdPartyStorageAccessGranted);

        // evil2
        _WKResourceLoadStatisticsThirdParty *evil2ThirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(evil2ThirdParty.thirdPartyDomain, @"evil2.com");

        NSEnumerator *evil2Enumerator = [evil2ThirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *evil2FirstParty1 = [evil2Enumerator nextObject];

        EXPECT_WK_STREQ(evil2FirstParty1.firstPartyDomain, @"webkit.org");
        EXPECT_FALSE(evil2FirstParty1.thirdPartyStorageAccessGranted);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, GetResourceLoadStatisticsDataSummaryDatabase)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView1 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView1 _test_waitForDidFinishNavigation];
    [webView2 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit2.org"]];
    [webView2 _test_waitForDidFinishNavigation];
    [webView3 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit3.org"]];
    [webView3 _test_waitForDidFinishNavigation];

    [dataStore _setResourceLoadStatisticsEnabled:YES];

    static bool doneFlag = false;
    [dataStore _setUseITPDatabase:true completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    
    doneFlag = false;
    [dataStore _setThirdPartyCookieBlockingMode:true onlyOnSitesWithoutUserInteraction:false completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    [dataStore _clearResourceLoadStatistics:^(void) {
        doneFlag = true;
    }];

    static bool statisticsUpdated = false;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        if (![message isEqualToString:@"Statistics Updated"])
            return;
        statisticsUpdated = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Set two third parties to be prevalent, leave evil1.com as non-prevalent to ensure
    // this call returns all third parties.
    doneFlag = false;
    [dataStore _setPrevalentDomain:[NSURL URLWithString:@"http://evil2.com"] completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    [dataStore _setPrevalentDomain:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Capture time for comparison later.
    NSTimeInterval beforeUpdatingTime = [[NSDate date] timeIntervalSince1970];

    // Seed test data in the web process' observer.

    // evil1
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    // evil2
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil2.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    // evil3
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit3.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    statisticsUpdated = false;
    [webView1 loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView2 loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit2.org"]];
    [webView3 loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit3.org"]];

    // Wait for the statistics to be updated in the network process.
    TestWebKitAPI::Util::run(&statisticsUpdated);

    // Check that the third-party evil1 is now registered as subresource.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil1.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    // Check that the third-party evil2 is now registered as subresource.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil2.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    // Check that the third-party evil3 is now registered as subresource.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit2.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://webkit3.org"] thirdParty:[NSURL URLWithString:@"http://evil3.com"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    // Collect the ITP data summary which should include all third parties in the
    // order: [evil3.com, evil1.com, evil2.com] sorted by number of first parties
    // it appears under or redirects to.
    doneFlag = false;
    [dataStore _getResourceLoadStatisticsDataSummary:^void(NSArray<_WKResourceLoadStatisticsThirdParty *> *thirdPartyData)
    {
        EXPECT_EQ(static_cast<int>([thirdPartyData count]), 3);
        NSEnumerator *thirdPartyDomains = [thirdPartyData objectEnumerator];

        // evil3
        _WKResourceLoadStatisticsThirdParty *evil3ThirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(evil3ThirdParty.thirdPartyDomain, @"evil3.com");

        NSEnumerator *evil3Enumerator = [evil3ThirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *evil3FirstParty1 = [evil3Enumerator nextObject];
        _WKResourceLoadStatisticsFirstParty *evil3FirstParty2 = [evil3Enumerator nextObject];
        _WKResourceLoadStatisticsFirstParty *evil3FirstParty3 = [evil3Enumerator nextObject];

        EXPECT_WK_STREQ(evil3FirstParty1.firstPartyDomain, @"webkit2.org");
        EXPECT_WK_STREQ(evil3FirstParty2.firstPartyDomain, @"webkit3.org");
        EXPECT_WK_STREQ(evil3FirstParty3.firstPartyDomain, @"webkit.org");

        EXPECT_FALSE(evil3FirstParty1.thirdPartyStorageAccessGranted);
        EXPECT_FALSE(evil3FirstParty2.thirdPartyStorageAccessGranted);
        EXPECT_FALSE(evil3FirstParty3.thirdPartyStorageAccessGranted);

        NSTimeInterval rightNow = [[NSDate date] timeIntervalSince1970];

        // Check timestamp for evil3 is reported as being within the correct range.
        NSTimeInterval evil1TimeLastUpdated = evil3FirstParty1.timeLastUpdated;
        NSTimeInterval evil2TimeLastUpdated = evil3FirstParty2.timeLastUpdated;
        NSTimeInterval evil3TimeLastUpdated = evil3FirstParty3.timeLastUpdated;

        EXPECT_TRUE(beforeUpdatingTime < evil1TimeLastUpdated);
        EXPECT_TRUE(beforeUpdatingTime < evil2TimeLastUpdated);
        EXPECT_TRUE(beforeUpdatingTime < evil3TimeLastUpdated);

        NSTimeInterval evil1HourDifference = (rightNow - evil1TimeLastUpdated) / 3600;
        NSTimeInterval evil2HourDifference = (rightNow - evil2TimeLastUpdated) / 3600;
        NSTimeInterval evil3HourDifference = (rightNow - evil3TimeLastUpdated) / 3600;

        EXPECT_TRUE(evil1HourDifference < 24*3600);
        EXPECT_TRUE(evil2HourDifference < 24*3600);
        EXPECT_TRUE(evil3HourDifference < 24*3600);

        // evil1
        _WKResourceLoadStatisticsThirdParty *evil1ThirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(evil1ThirdParty.thirdPartyDomain, @"evil1.com");

        NSEnumerator *evil1Enumerator = [evil1ThirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *evil1FirstParty1= [evil1Enumerator nextObject];
        _WKResourceLoadStatisticsFirstParty *evil1FirstParty2 = [evil1Enumerator nextObject];

        EXPECT_WK_STREQ(evil1FirstParty1.firstPartyDomain, @"webkit2.org");
        EXPECT_WK_STREQ(evil1FirstParty2.firstPartyDomain, @"webkit.org");

        EXPECT_FALSE(evil1FirstParty1.thirdPartyStorageAccessGranted);
        EXPECT_FALSE(evil1FirstParty2.thirdPartyStorageAccessGranted);

        // Check timestamp for evil1 is reported as being within the correct range.
        evil1TimeLastUpdated = evil1FirstParty1.timeLastUpdated;
        evil2TimeLastUpdated = evil1FirstParty2.timeLastUpdated;

        EXPECT_TRUE(beforeUpdatingTime < evil1TimeLastUpdated);
        EXPECT_TRUE(beforeUpdatingTime < evil2TimeLastUpdated);

        evil1HourDifference = (rightNow - evil1TimeLastUpdated) / 3600;
        evil2HourDifference = (rightNow - evil2TimeLastUpdated) / 3600;

        EXPECT_TRUE(evil1HourDifference < 24*3600);
        EXPECT_TRUE(evil2HourDifference < 24*3600);

        // evil2
        _WKResourceLoadStatisticsThirdParty *evil2ThirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(evil2ThirdParty.thirdPartyDomain, @"evil2.com");

        NSEnumerator *evil2Enumerator = [evil2ThirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *evil2FirstParty1 = [evil2Enumerator nextObject];

        EXPECT_WK_STREQ(evil2FirstParty1.firstPartyDomain, @"webkit.org");
        EXPECT_FALSE(evil2FirstParty1.thirdPartyStorageAccessGranted);

        // Check timestamp for evil2 is reported as being within the correct range.
        evil1TimeLastUpdated = evil1FirstParty1.timeLastUpdated;

        EXPECT_TRUE(beforeUpdatingTime < evil1TimeLastUpdated);

        evil1HourDifference = (rightNow - evil1TimeLastUpdated) / 3600;

        EXPECT_TRUE(evil1HourDifference < 24*3600);

        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, MigrateDataFromIncorrectCreateTableSchema)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    auto defaultFileManager = [NSFileManager defaultManager];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    NSURL *itpRootURL = [[dataStore _configuration] _resourceLoadStatisticsDirectory];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load an incorrect database schema with pre-seeded ITP data.
    // This data should be migrated into the new database.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [[NSBundle mainBundle] URLForResource:@"incorrectCreateTableSchema" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newFileURL.path]);
    [defaultFileManager copyItemAtPath:newFileURL.path toPath:fileURL.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:fileURL.path]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    static bool doneFlag;
    [dataStore _setUseITPDatabase:true completionHandler: ^(void) {
        doneFlag = true;
    }];
    
    TestWebKitAPI::Util::run(&doneFlag);
    
    // Check that the pre-seeded data is in the new database after migrating.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://apple.com"] thirdParty:[NSURL URLWithString:@"http://webkit.org"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    
    // To make sure creation of unique indices is maintained, try to insert the same data
    // again and make sure the timestamp is replaced, and there is only one entry.
    static bool statisticsUpdated = false;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        if (![message isEqualToString:@"Statistics Updated"])
            return;
        statisticsUpdated = true;
    }];
    
    doneFlag = false;
    [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://apple.com"] thirdParty:[NSURL URLWithString:@"http://webkit.org"] shouldScheduleNotification:NO completionHandler: ^() {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    
    statisticsUpdated = false;
    [webView loadHTMLString:@"<body><script>close();</script></body>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    // Wait for the statistics to be updated in the network process.
    TestWebKitAPI::Util::run(&statisticsUpdated);

    doneFlag = false;
    [dataStore _getResourceLoadStatisticsDataSummary:^void(NSArray<_WKResourceLoadStatisticsThirdParty *> *thirdPartyData)
    {
        EXPECT_EQ(static_cast<int>([thirdPartyData count]), 1);
        NSEnumerator *thirdPartyDomains = [thirdPartyData objectEnumerator];

        _WKResourceLoadStatisticsThirdParty *thirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(thirdParty.thirdPartyDomain, @"webkit.org");

        NSEnumerator *enumerator = [thirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *firstParty = [enumerator nextObject];

        EXPECT_WK_STREQ(firstParty.firstPartyDomain, @"apple.com");

        // Check timestamp for first party was updated.
        NSTimeInterval firstPartyLastUpdated = firstParty.timeLastUpdated;
        EXPECT_GT(firstPartyLastUpdated, 0);

        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, MigrateDataFromMissingTopFrameUniqueRedirectSameSiteStrictTableSchema)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    auto defaultFileManager = [NSFileManager defaultManager];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    NSURL *itpRootURL = [[dataStore _configuration] _resourceLoadStatisticsDirectory];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load an incorrect database schema with pre-seeded ITP data.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [[NSBundle mainBundle] URLForResource:@"missingTopFrameUniqueRedirectSameSiteStrictTableSchema" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newFileURL.path]);
    [defaultFileManager copyItemAtPath:newFileURL.path toPath:fileURL.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:fileURL.path]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool: sharedProcessPool];
    configuration.get().websiteDataStore = dataStore;

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    static bool doneFlag;
    [dataStore _setUseITPDatabase:true completionHandler: ^(void) {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Since the database should not be deleted, the pre-seeded data should
    // still be there after initializing ITP.
    doneFlag = false;
    [dataStore _isRegisteredAsSubresourceUnderFirstParty:[NSURL URLWithString:@"http://apple.com"] thirdParty:[NSURL URLWithString:@"http://webkit.org"] completionHandler: ^(BOOL isRegistered) {
        EXPECT_TRUE(isRegistered);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    
    // Check to make sure all tables are accounted for.
    doneFlag = false;
    [dataStore _statisticsDatabaseHasAllTables:^(BOOL hasAllTables) {
        EXPECT_TRUE(hasAllTables);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, CanAccessDataSummaryWithNoProcessPool)
{
    auto defaultFileManager = [NSFileManager defaultManager];
    _WKWebsiteDataStoreConfiguration *dataStoreConfiguration = [[_WKWebsiteDataStoreConfiguration new] autorelease];
    auto *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration] autorelease];
    NSURL *itpRootURL = [[dataStore _configuration] _resourceLoadStatisticsDirectory];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load a a pre-seeded ITP database.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [[NSBundle mainBundle] URLForResource:@"basicITPDatabase" withExtension:@"db" subdirectory:@"TestWebKitAPI.resources"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newFileURL.path]);
    [defaultFileManager copyItemAtPath:newFileURL.path toPath:fileURL.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:fileURL.path]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    static bool doneFlag = false;
    [dataStore _getResourceLoadStatisticsDataSummary:^void(NSArray<_WKResourceLoadStatisticsThirdParty *> *thirdPartyData)
    {
        EXPECT_EQ(static_cast<int>([thirdPartyData count]), 1);
        NSEnumerator *thirdPartyDomains = [thirdPartyData objectEnumerator];

        _WKResourceLoadStatisticsThirdParty *thirdParty = [thirdPartyDomains nextObject];
        EXPECT_WK_STREQ(thirdParty.thirdPartyDomain, @"webkit.org");

        NSEnumerator *enumerator = [thirdParty.underFirstParties objectEnumerator];
        _WKResourceLoadStatisticsFirstParty *firstParty = [enumerator nextObject];

        EXPECT_WK_STREQ(firstParty.firstPartyDomain, @"apple.com");

        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
}

TEST(ResourceLoadStatistics, StoreSuspension)
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];
    auto *dataStore1 = [WKWebsiteDataStore defaultDataStore];
    auto *dataStore2 = [[[WKWebsiteDataStore alloc] _initWithConfiguration:[[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease]] autorelease];

    [dataStore1 _setResourceLoadStatisticsEnabled:YES];
    [dataStore2 _setResourceLoadStatisticsEnabled:YES];

    static bool doneFlag = false;
    [dataStore1 _setUseITPDatabase:true completionHandler: ^(void) {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    [dataStore2 _setUseITPDatabase:true completionHandler: ^(void) {
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    auto configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration1 setProcessPool: sharedProcessPool];
    configuration1.get().websiteDataStore = dataStore1;

    auto configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration2 setProcessPool: sharedProcessPool];
    configuration2.get().websiteDataStore = dataStore2;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration1.get()]);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration2.get()]);

    [webView1 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView1 _test_waitForDidFinishNavigation];

    [webView2 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit2.org"]];
    [webView2 _test_waitForDidFinishNavigation];

    doneFlag = false;
    [sharedProcessPool _sendNetworkProcessPrepareToSuspend:^{
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    [sharedProcessPool _sendNetworkProcessDidResume];
}

@interface ResourceLoadStatisticsSchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation ResourceLoadStatisticsSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    NSString *response = nil;
    if ([task.request.URL.path isEqualToString:@"/fp-load-one"])
        response = @"<body><iframe src='resource-load-statistics://example1.com/tp-load'></iframe></body>";
    if ([task.request.URL.path isEqualToString:@"/fp-load-two"])
        response = @"<body><iframe src='resource-load-statistics://example2.com/tp-load'></iframe></body>";
    else if ([task.request.URL.path isEqualToString:@"/tp-load"])
        response = @"<body></body>";

    [task didReceiveResponse:[[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:response.length textEncodingName:nil] autorelease]];
    [task didReceiveData:[response dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

TEST(ResourceLoadStatistics, BackForwardPerPageData)
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    auto delegate = adoptNS([TestNavigationDelegate new]);

    auto schemeHandler = adoptNS([[ResourceLoadStatisticsSchemeHandler alloc] init]);
    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"resource-load-statistics"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView setNavigationDelegate:delegate.get()];

    static bool doneFlag = false;
    [dataStore _loadedSubresourceDomainsFor:webView.get() completionHandler:^(NSArray<NSString *> *domains) {
        EXPECT_EQ(static_cast<int>([domains count]), 0);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Seed the page with a third party.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"resource-load-statistics://fp1/fp-load-one"]]];
    [delegate waitForDidFinishNavigation];

    doneFlag = false;
    [dataStore _loadedSubresourceDomainsFor:webView.get() completionHandler:^(NSArray<NSString *> *domains) {
        EXPECT_EQ(static_cast<int>([domains count]), 1);
        EXPECT_WK_STREQ([domains objectAtIndex:0], @"example1.com");
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Navigate somewhere else and load a different third party.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"resource-load-statistics://fp2/fp-load-two"]]];
    [delegate waitForDidFinishNavigation];

    doneFlag = false;
    [dataStore _loadedSubresourceDomainsFor:webView.get() completionHandler:^(NSArray<NSString *> *domains) {
        EXPECT_EQ(static_cast<int>([domains count]), 1);
        EXPECT_WK_STREQ([domains objectAtIndex:0], @"example2.com");
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Go back, check for the cached third party example1.com.
    [webView goBack];
    [delegate waitForDidFinishNavigation];

    doneFlag = false;
    [dataStore _loadedSubresourceDomainsFor:webView.get() completionHandler:^(NSArray<NSString *> *domains) {
        EXPECT_EQ(static_cast<int>([domains count]), 1);
        EXPECT_WK_STREQ([domains objectAtIndex:0], @"example1.com");
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);

    // Go forward, check for the cached third party example2.com.
    [webView goForward];
    [delegate waitForDidFinishNavigation];

    doneFlag = false;
    [dataStore _loadedSubresourceDomainsFor:webView.get() completionHandler:^(NSArray<NSString *> *domains) {
        EXPECT_EQ(static_cast<int>([domains count]), 1);
        EXPECT_WK_STREQ([domains objectAtIndex:0], @"example2.com");
        doneFlag = true;
    }];
    
    TestWebKitAPI::Util::run(&doneFlag);
}
