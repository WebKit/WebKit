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
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/SQLiteDatabase.h>
#import <WebCore/SQLiteStatement.h>
#import <WebKit/WKFoundation.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/text/MakeString.h>

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

static void ensureITPFileIsCreated()
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [dataStore _setResourceLoadStatisticsEnabled:NO];
}

// FIXME when rdar://109481486 is resolved rdar://134535336
#if PLATFORM(IOS) || PLATFORM(VISION) || PLATFORM(MAC)
TEST(ResourceLoadStatistics, DISABLED_GrandfatherCallback)
#else
TEST(ResourceLoadStatistics, GrandfatherCallback)
#endif
{
    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get().pcmMachServiceName = nil;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *fileURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"observations.db"];
    [[NSFileManager defaultManager] removeItemAtURL:fileURL error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:statisticsDirectoryURL error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:statisticsDirectoryURL.path]);

    static bool grandfatheredFlag;
    static bool doneFlag;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        ASSERT_TRUE([message isEqualToString:@"Grandfathered"]);
        grandfatheredFlag = true;
    }];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

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

    ensureITPFileIsCreated();

    __block bool callbackFlag = false;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        callbackFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    TestWebKitAPI::Util::run(&callbackFlag);
}

TEST(ResourceLoadStatistics, ChildProcessesNotLaunched)
{
    // Ensure the shared process pool exists so the data store operations we're about to do work with it.
    WKProcessPool *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    EXPECT_EQ((size_t)0, [sharedProcessPool _pluginProcessCount]);

    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *targetURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"observations.db"];

    ensureITPFileIsCreated();

    __block bool callbackFlag = false;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        // Only if the database store is default-enabled do we need to check that this first message is correct.
        EXPECT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        callbackFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

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

    [webView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"notify-resourceLoadObserver" withExtension:@"html"]]];

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

    [configuration.get().websiteDataStore _terminateNetworkProcess];

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

    [configuration.get().websiteDataStore _terminateNetworkProcess];

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
- (Vector<String>)waitForMessages:(size_t)count;
@end

@implementation DataTaskIdentifierCollisionDelegate {
    Vector<String> _messages;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    _messages.append(message);
    completionHandler();
}

- (Vector<String>)waitForMessages:(size_t)messageCount
{
    while (_messages.size() < messageCount)
        TestWebKitAPI::Util::spinRunLoop();
    return std::exchange(_messages, { });
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * credential))completionHandler
{
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}

@end

void waitUntilTwoServersConnected(const unsigned& serversConnected, CompletionHandler<void()>&& completionHandler)
{
    if (serversConnected >= 2) {
        completionHandler();
        return;
    }
    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([&serversConnected, completionHandler = WTFMove(completionHandler)] () mutable {
        waitUntilTwoServersConnected(serversConnected, WTFMove(completionHandler));
    }).get());
}

TEST(ResourceLoadStatistics, DataTaskIdentifierCollision)
{
    using namespace TestWebKitAPI;

    unsigned serversConnected { 0 };
    constexpr auto header = "HTTP/1.1 200 OK\r\nContent-Length: 27\r\n\r\n"_s;

    HTTPServer httpsServer([&] (const Connection& connection) {
        serversConnected++;
        waitUntilTwoServersConnected(serversConnected, [=] {
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                connection.send(makeString(header, "<script>alert('1')</script>"_s));
            });
        });
    }, HTTPServer::Protocol::HttpsProxy);

    HTTPServer httpServer([&] (const Connection& connection) {
        serversConnected++;
        waitUntilTwoServersConnected(serversConnected, [=] {
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                connection.send(makeString(header, "<script>alert('2')</script>"_s));
            });
        });
    });

    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    storeConfiguration.get().httpsProxy = [NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", httpsServer.port()]];
    storeConfiguration.get().allowsServerPreconnect = NO;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get() addToWindow:NO]);
    [webView1 synchronouslyLoadHTMLString:@"start network process and initialize data store"];
    auto delegate = adoptNS([DataTaskIdentifierCollisionDelegate new]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get() addToWindow:NO]);
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

    auto messages = [delegate waitForMessages:2];
    auto contains = [] (const Vector<String>& array, ASCIILiteral expected) {
        for (auto& s : array) {
            if (s == expected)
                return true;
        }
        return false;
    };
    EXPECT_TRUE(contains(messages, "1"_s));
    EXPECT_TRUE(contains(messages, "2"_s));
}

TEST(ResourceLoadStatistics, NoMessagesWhenNotTesting)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]).get()]);
    [configuration setWebsiteDataStore:dataStore.get()];
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

    __block bool doneFlag = false;
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

// rdar://134535336
#if PLATFORM(IOS) || PLATFORM(MAC)
TEST(ResourceLoadStatistics, DISABLED_MigrateDataFromIncorrectCreateTableSchema)
#else
TEST(ResourceLoadStatistics, MigrateDataFromIncorrectCreateTableSchema)
#endif
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
    NSURL *newFileURL = [NSBundle.test_resourcesBundle URLForResource:@"incorrectCreateTableSchema" withExtension:@"db"];
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

    __block bool doneFlag = false;
    // Check that the pre-seeded data is in the new database after migrating.
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

// rdar://136535465
#if PLATFORM(MAC)
TEST(ResourceLoadStatistics, DISABLED_MigrateDataFromMissingTopFrameUniqueRedirectSameSiteStrictTableSchema)
#else
TEST(ResourceLoadStatistics, MigrateDataFromMissingTopFrameUniqueRedirectSameSiteStrictTableSchema)
#endif
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
    NSURL *newFileURL = [NSBundle.test_resourcesBundle URLForResource:@"missingTopFrameUniqueRedirectSameSiteStrictTableSchema" withExtension:@"db"];
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

    __block bool doneFlag = false;
    // Since the database should not be deleted, the pre-seeded data should
    // still be there after initializing ITP.
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
    NSURL *itpRootURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"CanAccessDataSummaryWithNoProcessPoolTest"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load a a pre-seeded ITP database.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:newFileURL.path]);
    [defaultFileManager copyItemAtPath:newFileURL.path toPath:fileURL.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:fileURL.path]);

    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    dataStoreConfiguration.get()._resourceLoadStatisticsDirectory = itpRootURL;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
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
    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    auto customGeneralStorageDirectory = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@%@", dataStoreConfiguration.get().generalStorageDirectory.path, @"_Custom"]];
    auto dataStore1 = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    dataStoreConfiguration.get().generalStorageDirectory = customGeneralStorageDirectory;
    auto dataStore2 = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);

    [dataStore1 _setResourceLoadStatisticsEnabled:YES];
    [dataStore2 _setResourceLoadStatisticsEnabled:YES];

    auto configuration1 = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration1 setProcessPool: sharedProcessPool];
    configuration1.get().websiteDataStore = dataStore1.get();

    auto configuration2 = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration2 setProcessPool: sharedProcessPool];
    configuration2.get().websiteDataStore = dataStore2.get();

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration1.get()]);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration2.get()]);

    [webView1 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView1 _test_waitForDidFinishNavigation];

    [webView2 loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit2.org"]];
    [webView2 _test_waitForDidFinishNavigation];

    __block bool doneFlag = false;
    [dataStore1 _sendNetworkProcessPrepareToSuspend:^{
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    [dataStore1 _sendNetworkProcessDidResume];
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

    [task didReceiveResponse:adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:response.length textEncodingName:nil]).get()];
    [task didReceiveData:[response dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

#if PLATFORM(MAC)
TEST(ResourceLoadStatistics, DataSummaryWithCachedProcess)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    processPoolConfiguration.get().prewarmsProcessesAutomatically = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    EXPECT_GT([processPool _maximumSuspendedPageCount], 0u);
    EXPECT_GT([processPool _processCacheCapacity], 0u);

    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    [WKWebsiteDataStore _setCachedProcessSuspensionDelayForTesting:0];

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    [webViewConfiguration setWebsiteDataStore:dataStore];

    auto handler = adoptNS([[ResourceLoadStatisticsSchemeHandler alloc] init]);

    const unsigned maxSuspendedPageCount = [processPool _maximumSuspendedPageCount];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"resource-load-statistics"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    for (unsigned i = 0; i < maxSuspendedPageCount + 1; i++) {
        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:makeString("resource-load-statistics://www.domain-"_s, i, ".com"_s)]];
        [webView loadRequest:request];
        [delegate waitForDidFinishNavigation];

        EXPECT_EQ(i + 1, [processPool _webProcessCount]);
        EXPECT_EQ(i + 1, [processPool _webProcessCountIgnoringPrewarmedAndCached]);
        EXPECT_FALSE([processPool _hasPrewarmedWebProcess]);
    }
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:makeString("resource-load-statistics://www.domain-"_s, maxSuspendedPageCount + 1, ".com"_s)]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];

    // This will timeout if waiting for IPC from a cached process.
    static bool doneFlag = false;
    [dataStore _getResourceLoadStatisticsDataSummary:^void(NSArray<_WKResourceLoadStatisticsThirdParty *> *thirdPartyData)
    {
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    
    [WKWebsiteDataStore _setCachedProcessSuspensionDelayForTesting:30];
}
#endif

TEST(ResourceLoadStatistics, BackForwardPerPageData)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    auto delegate = adoptNS([TestNavigationDelegate new]);

    auto schemeHandler = adoptNS([[ResourceLoadStatisticsSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"resource-load-statistics"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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

// rdar://136535465
#if PLATFORM(MAC)
TEST(ResourceLoadStatistics, DISABLED_MigrateDistinctDataFromTableWithMissingIndexes)
#else
TEST(ResourceLoadStatistics, MigrateDistinctDataFromTableWithMissingIndexes)
#endif
{
    auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    auto defaultFileManager = [NSFileManager defaultManager];
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];
    NSURL *itpRootURL = [[dataStore _configuration] _resourceLoadStatisticsDirectory];
    NSURL *fileURL = [itpRootURL URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpRootURL.path]);

    // Load an incorrect database schema with pre-seeded ITP data. In this case, it contains various entries of webkit.org and apple.com
    // repeatedly stored as subframes, subresources, and unique redirects. It also has them as repeated source and destination sites for PCM.
    // Once we add unique indexes, each entry should be migrated exactly once. Debug asserts when running the test will indicate failure.
    [defaultFileManager createDirectoryAtURL:itpRootURL withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *newFileURL = [NSBundle.test_resourcesBundle URLForResource:@"resourceLoadStatisticsMissingUniqueIndex" withExtension:@"db"];
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

    __block bool doneFlag = false;
    // Check that the webkit.org is stored as appearing under apple.com exactly once.
    [dataStore _isRelationshipOnlyInDatabaseOnce:[NSURL URLWithString:@"http://apple.com"] thirdParty:[NSURL URLWithString:@"http://webkit.org"] completionHandler:^void(BOOL result) {
        EXPECT_TRUE(result);
        doneFlag = true;
    }];
    TestWebKitAPI::Util::run(&doneFlag);

    doneFlag = false;
    // Check that the apple.com is stored as appearing under webkit.org exactly once.
    [dataStore _isRelationshipOnlyInDatabaseOnce:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://apple.com"] completionHandler:^void(BOOL result) {
        EXPECT_TRUE(result);
        doneFlag = true;
    }];

    TestWebKitAPI::Util::run(&doneFlag);
    
    // Clear pre-filled database.
    [defaultFileManager removeItemAtPath:itpRootURL.path error:nil];
}

static Vector<String> columnsForTable(WebCore::SQLiteDatabase& database, ASCIILiteral tableName)
{
    auto statement = database.prepareStatementSlow(makeString("PRAGMA table_info("_s, tableName, ')'));
    EXPECT_NOT_NULL(statement);

    Vector<String> columns;
    while (statement->step() == SQLITE_ROW)
        columns.append(statement->columnText(1));

    return columns;
}

TEST(ResourceLoadStatistics, DatabaseSchemeUpdate)
{
    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);

    NSError *error = nil;
    [[NSFileManager defaultManager] removeItemAtURL:dataStoreConfiguration.get()._resourceLoadStatisticsDirectory error:&error];
    EXPECT_NULL(error);
    bool createdDirectory = [[NSFileManager defaultManager] createDirectoryAtURL:dataStoreConfiguration.get()._resourceLoadStatisticsDirectory withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_TRUE(createdDirectory);
    EXPECT_NULL(error);
    
    NSURL *targetURL = [dataStoreConfiguration.get()._resourceLoadStatisticsDirectory URLByAppendingPathComponent:@"observations.db"];
    WebCore::SQLiteDatabase database;
    EXPECT_TRUE(database.open(targetURL.path));

    constexpr auto createObservedDomain = "CREATE TABLE ObservedDomains ("
        "domainID INTEGER PRIMARY KEY, registrableDomain TEXT NOT NULL UNIQUE ON CONFLICT FAIL, lastSeen REAL NOT NULL, "
        "hadUserInteraction INTEGER NOT NULL, mostRecentUserInteractionTime REAL NOT NULL, grandfathered INTEGER NOT NULL, "
        "isPrevalent INTEGER NOT NULL, isVeryPrevalent INTEGER NOT NULL, dataRecordsRemoved INTEGER NOT NULL,"
        "timesAccessedAsFirstPartyDueToUserInteraction INTEGER NOT NULL, timesAccessedAsFirstPartyDueToStorageAccessAPI INTEGER NOT NULL,"
        "isScheduledForAllButCookieDataRemoval INTEGER NOT NULL)"_s;

    EXPECT_TRUE(database.executeCommand(createObservedDomain));
    database.close();

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    __block bool done { false };
    [dataStore _setPrevalentDomain:[NSURL URLWithString:@"https://example.com/"] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    
    WebCore::SQLiteDatabase databaseAfterMigration;
    EXPECT_TRUE(databaseAfterMigration.open(targetURL.path));
    auto columns = columnsForTable(databaseAfterMigration, "ObservedDomains"_s);
    databaseAfterMigration.close();
    EXPECT_WK_STREQ(columns.last(), "mostRecentWebPushInteractionTime");
}

// rdar://136525714
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 150000
TEST(ResourceLoadStatistics, DISABLED_ClientEvaluatedJavaScriptDoesNotLogUserInteraction)
#else
TEST(ResourceLoadStatistics, ClientEvaluatedJavaScriptDoesNotLogUserInteraction)
#endif
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]).get()]);
    [configuration setWebsiteDataStore:dataStore.get()];
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    [dataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeResourceLoadStatistics] modifiedSince:[NSDate distantPast] completionHandler:^ {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"" completionHandler:^(id value, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeResourceLoadStatistics] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 0u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

// rdar://136525714
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 150000
TEST(ResourceLoadStatistics, DISABLED_UserGestureLogsUserInteraction)
#else
TEST(ResourceLoadStatistics, UserGestureLogsUserInteraction)
#endif
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]).get()]);
    [configuration setWebsiteDataStore:dataStore.get()];
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration]);
    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    [dataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeResourceLoadStatistics] modifiedSince:[NSDate distantPast] completionHandler:^ {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"internals.withUserGesture(() => {});" completionHandler:^(id value, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeResourceLoadStatistics] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 1u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ResourceLoadStatistics, UserAgentStringForSite)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]).get()]);
    [configuration setWebsiteDataStore:dataStore.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto userAgent = @"NotARealUserAgent";

    __block bool done = false;
    [dataStore _setUserAgentStringQuirkForTesting:@"webkit.org" withUserAgent:userAgent completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        EXPECT_WK_STREQ(action.request.allHTTPHeaderFields[@"User-Agent"], userAgent);
        decisionHandler(WKNavigationActionPolicyCancel);
        done = true;
    };
    [webView setNavigationDelegate:delegate.get()];

    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&done);
    done = false;

    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        EXPECT_FALSE([action.request.allHTTPHeaderFields[@"User-Agent"] isEqual:userAgent]);
        decisionHandler(WKNavigationActionPolicyCancel);
        done = true;
    };
    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://not-webkit.org"]];
    TestWebKitAPI::Util::run(&done);
}

TEST(ResourceLoadStatistics, StorageAccessPromptSiteWithQuirk)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]).get()]);
    [configuration setWebsiteDataStore:dataStore.get()];

    [dataStore _setResourceLoadStatisticsEnabled:YES];

    __block bool done = false;
    [dataStore _clearResourceLoadStatistics:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _setStorageAccessPromptQuirkForTesting:@"site1.example" withSubFrameDomains:[NSArray arrayWithObject:@"site2.example"] withTriggerPages:@[] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://site1.example"]];
    [webView _test_waitForDidFinishNavigation];

    __block bool navigationDelegateDone = false;
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidPromptForStorageAccess:^(WKWebView *, NSString *topFrameDomain, NSString *subFrameDomain, BOOL hasQuirk) {
        EXPECT_TRUE(hasQuirk);
        navigationDelegateDone = true;
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"let iframe = document.createElement(\"iframe\"); iframe.src = \"https://www.site2.example\"; document.body.appendChild(iframe);" completionHandler:^(id value, NSError *error) {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&navigationDelegateDone);
}

TEST(ResourceLoadStatistics, StorageAccessPromptSiteWithTrigger)
{
    using namespace TestWebKitAPI;
    HTTPServer httpServer({
        { "http://site1.example/page1"_s, { "<body>Done</body>"_s  } },
        { "http://site1.example/page2"_s, { "<body>Done</body>"_s  } },
        { "http://site1.example/page3"_s, { "<body>Done</body>"_s  } },
        { "http://site2.example/page1"_s, { "<body><script>alert(\"Loaded\");</script></body>"_s  } },
        { "http://site2.example/page2"_s, { "<body>iframe Body</body>"_s  } },
        { "http://site3.example/page1"_s, { "<body><script>if (window.internals) { internals.withUserGesture(() => { document.requestStorageAccess(); }); document.body.innerText = \"Requesting storage access\"; } else document.body.innerText = \"Internals not present\";</script></body>"_s  } },
    });

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
    }];

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);

    [dataStore _setResourceLoadStatisticsEnabled:YES];

    __block bool done = false;
    [dataStore _clearResourceLoadStatistics:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _setStorageAccessPromptQuirkForTesting:@"site1.example" withSubFrameDomains:[NSArray arrayWithObject:@"site2.example"] withTriggerPages:@[@"http://site1.example/page2"] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration setWebsiteDataStore:dataStore.get()];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    __block bool gotRequestStorageAccessPanelForQuirksForDomain { false };
    uiDelegate.get().requestStorageAccessPanelForQuirksForDomain = ^(WKWebView *, NSString *, NSString *, NSDictionary<NSString *, NSArray<NSString *> *> *, void  (^completionHandler)(BOOL)) {
        gotRequestStorageAccessPanelForQuirksForDomain = true;
        completionHandler(NO);
    };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration]);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    __block bool didReceiveStorageAccessPrompt = false;
    [navigationDelegate setDidPromptForStorageAccess:^(WKWebView *webview, NSString *topFrameDomain, NSString *subFrameDomain, BOOL hasQuirk) {
        if ([webView.get().URL.absoluteString isEqualToString:@"http://site1.example/page1"] || [webView.get().URL.absoluteString isEqualToString:@"http://site1.example/page2"])
            EXPECT_EQ(hasQuirk, YES);
        else
            EXPECT_EQ(hasQuirk, NO);
        didReceiveStorageAccessPrompt = true;
    }];

    __block bool finishedNavigation = false;
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView setUIDelegate:uiDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/page1"]]];

    Util::run(&finishedNavigation);
    finishedNavigation = false;

    [webView evaluateJavaScript:@"let iframe = document.createElement(\"iframe\"); iframe.src = \"http://site2.example/page1\"; document.body.appendChild(iframe);" completionHandler:^(id value, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"Loaded");
    EXPECT_FALSE(gotRequestStorageAccessPanelForQuirksForDomain);
    didReceiveStorageAccessPrompt = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/page2"]]];
    Util::run(&finishedNavigation);
    finishedNavigation = false;

    [webView evaluateJavaScript:@"let iframe = document.createElement(\"iframe\"); iframe.src = \"http://site2.example/page2\"; document.body.appendChild(iframe);" completionHandler:^(id value, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&didReceiveStorageAccessPrompt);
    EXPECT_TRUE(gotRequestStorageAccessPanelForQuirksForDomain);
    done = false;
    didReceiveStorageAccessPrompt = false;
    gotRequestStorageAccessPanelForQuirksForDomain = false;

    [dataStore _logUserInteraction:[NSURL URLWithString:@"http://site3.example/"] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/page3"]]];
    Util::run(&finishedNavigation);
    finishedNavigation = false;

    [webView evaluateJavaScript:@"let iframe = document.createElement(\"iframe\"); iframe.src = \"http://site3.example/page1\"; document.body.appendChild(iframe);" completionHandler:^(id value, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    TestWebKitAPI::Util::run(&didReceiveStorageAccessPrompt);
    EXPECT_FALSE(gotRequestStorageAccessPanelForQuirksForDomain);
    done = false;
    didReceiveStorageAccessPrompt = false;
}

TEST(ResourceLoadStatistics, StorageAccessOnRedirectSitesWithOutQuirk)
{
    using namespace TestWebKitAPI;
    HTTPServer httpServer({
        { "http://site1.example/page1"_s, { "<body><script>let iframe = document.createElement(\"iframe\"); iframe.src = \"http://site2.example/page1\"; document.body.appendChild(iframe);</script></body>"_s  } },
        { "http://site2.example/page1"_s, { 302, {{ "Location"_s, "http://site3.example/page2"_s }}, "Redirecting"_s } },
        { "http://site3.example/page1"_s, { 200, {{ "Set-Cookie"_s, "exists=1;"_s }}, "Body"_s } },
        { "http://site3.example/page2"_s, { 200, {{ "Set-Cookie"_s, "exists=2;"_s }}, "Body"_s } },
        { "http://site3.example/page3"_s, { "<body>Done</body>"_s  } },
    });

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [configuration setWebsiteDataStore:dataStore.get()];
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    __block bool done = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        done = true;
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site3.example/page1"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;
    finishedNavigation = false;

    [webView evaluateJavaScript:@"document.cookie" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"exists=1", (NSString *)value);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/page1"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;
    finishedNavigation = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site3.example/page3"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;

    [webView evaluateJavaScript:@"document.cookie" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"exists=1", (NSString *)value);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ResourceLoadStatistics, StorageAccessOnRedirectSitesWithQuirk)
{
    using namespace TestWebKitAPI;
    HTTPServer httpServer({
        { "http://site1.example/page1"_s, { "<body><script>let iframe = document.createElement(\"iframe\"); iframe.src = \"http://site2.example/page1\"; document.body.appendChild(iframe);</script></body>"_s  } },
        { "http://site1.example/page2"_s, { "<body><script>let iframe = document.createElement(\"iframe\"); iframe.src = \"http://site4.example/page1\"; document.body.appendChild(iframe);</script></body>"_s  } },
        { "http://site2.example/page1"_s, { 302, { { "Location"_s, "http://site3.example/page2"_s }}, "Redirecting"_s } },
        { "http://site3.example/page1"_s, { 200, {{ "Set-Cookie"_s, "exists=1;"_s }}, "Body"_s } },
        { "http://site3.example/page2"_s, { 200, {{ "Set-Cookie"_s, "exists=2;"_s }}, "Body"_s } },
        { "http://site3.example/page3"_s, { "<body>Done</body>"_s  } },
        { "http://site4.example/page1"_s, { 200, {{ "Set-Cookie"_s, "exists=1;"_s }}, "Body"_s } },
        { "http://site4.example/page2"_s, { "<body>Done</body>"_s  } },
    });

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [configuration setWebsiteDataStore:dataStore.get()];
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    [dataStore _clearResourceLoadStatistics:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    __block bool done = false;
    [dataStore _setStorageAccessPromptQuirkForTesting:@"site1.example" withSubFrameDomains:[NSArray arrayWithObjects:@"site2.example", @"site3.example", nil] withTriggerPages:@[] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _grantStorageAccessForTesting:@"site1.example" withSubFrameDomains:[NSArray arrayWithObjects:@"site2.example", @"site3.example", nil] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        done = true;
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site3.example/page1"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;
    finishedNavigation = false;

    [webView evaluateJavaScript:@"document.cookie" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"exists=1", (NSString *)value);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/page1"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;
    finishedNavigation = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site3.example/page3"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;

    [webView evaluateJavaScript:@"document.cookie" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"exists=2", (NSString *)value);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/page2"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;
    finishedNavigation = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site4.example/page2"]]];
    TestWebKitAPI::Util::run(&done);
    Util::run(&finishedNavigation);
    done = false;
    finishedNavigation = false;

    [webView evaluateJavaScript:@"document.cookie" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"", (NSString *)value);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ResourceLoadStatistics, EnableResourceLoadStatisticsAfterNetworkProcessCreation)
{
    __block bool done = false;
    @autoreleasepool {
        // Create WebsiteDataStore to leave ITP data on disk.
        auto websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:[[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]];
        [websiteDataStore _setResourceLoadStatisticsEnabled:YES];
        [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate]  modifiedSince:[NSDate distantPast] completionHandler:^ {
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        auto *sharedProcessPool = [WKProcessPool _sharedProcessPool];
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        configuration.get().processPool = sharedProcessPool;
        configuration.get().websiteDataStore = websiteDataStore;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
        [webView _test_waitForDidFinishNavigation];

        done = false;
        [sharedProcessPool _seedResourceLoadStatisticsForTestingWithFirstParty:[NSURL URLWithString:@"http://webkit.org"] thirdParty:[NSURL URLWithString:@"http://evil.com"] shouldScheduleNotification:NO completionHandler: ^() {
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        done = false;
        [websiteDataStore _getResourceLoadStatisticsDataSummary:^(NSArray<_WKResourceLoadStatisticsThirdParty *> *thirdPartyData) {
            EXPECT_EQ([thirdPartyData count], 1u);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        pid_t networkProcessIdentifier = [websiteDataStore _networkProcessIdentifier];
        EXPECT_NE(networkProcessIdentifier, 0);
        kill(networkProcessIdentifier, SIGKILL);
    }

    auto websiteDataStore1 = [WKWebsiteDataStore dataStoreForIdentifier:[[NSUUID alloc] initWithUUIDString:@"940e7729-738e-439f-a366-1a8719e23b2d"]];
    [websiteDataStore1 _setResourceLoadStatisticsEnabled:NO];
    auto websiteDataStore2 = [WKWebsiteDataStore dataStoreForIdentifier:[[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]];
    [websiteDataStore2 _setResourceLoadStatisticsEnabled:NO];

    // Wait until new network process is launched.
    done = false;
    [websiteDataStore1 removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate]  modifiedSince:[NSDate distantPast] completionHandler:^ {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    while (![websiteDataStore1 _networkProcessIdentifier])
        TestWebKitAPI::Util::spinRunLoop(10);

    [websiteDataStore2 _setResourceLoadStatisticsEnabled:YES];
    done = false;
    [websiteDataStore2 _getResourceLoadStatisticsDataSummary:^(NSArray<_WKResourceLoadStatisticsThirdParty *> *thirdPartyData) {
        EXPECT_EQ([thirdPartyData count], 1u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ResourceLoadStatistics, StorageAccessSupportMultipleSubFrameDomains)
{
    using namespace TestWebKitAPI;

    HTTPServer httpServer { HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();

            URL url { HTTPServer::parsePath(request) };

            if (url.string() == "http://site1.example/"_s) {
                co_await connection.awaitableSend(HTTPResponse("<body></body>"_s).serialize());
                continue;
            }

            if ((url.host() == "site2.example"_s || url.host() == "site3.example"_s) && url.path() == "/request-storage-access"_s) {
                co_await connection.awaitableSend(HTTPResponse("<body><script>internals.withUserGesture(() => { document.body.innerText = \"User gesture\"; document.requestStorageAccess().then(() => { document.body.innerText = \"Granted access\"; alert(\"Granted\"); }) });</script>site page 1 iframe</body>"_s).serialize());
                continue;
            }

            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::Http };

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
    }];

    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [configuration setWebsiteDataStore:dataStore.get()];
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    __block bool done { false };
    [dataStore _clearResourceLoadStatistics:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _logUserInteraction:[NSURL URLWithString:@"http://site2.example/"] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _logUserInteraction:[NSURL URLWithString:@"http://site3.example/"] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _setStorageAccessPromptQuirkForTesting:@"site1.example" withSubFrameDomains:[NSArray arrayWithObject:@"site2.example"] withTriggerPages:@[] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    __block bool gotRequestStorageAccessPanelForDomain { false };
    __block bool gotRequestStorageAccessPanelForQuirksForDomain { false };
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().requestStorageAccessPanelForDomain = ^(WKWebView *, NSString *subFrameDomain, NSString *topFrameDomain, void  (^completionHandler)(BOOL)) {
        EXPECT_NOT_NULL(subFrameDomain);
        EXPECT_NOT_NULL(subFrameDomain);
        EXPECT_WK_STREQ(@"site3.example", subFrameDomain);
        EXPECT_WK_STREQ(@"site1.example", topFrameDomain);
        gotRequestStorageAccessPanelForDomain = true;
        completionHandler(YES);
    };

    uiDelegate.get().requestStorageAccessPanelForQuirksForDomain = ^(WKWebView *, NSString *subFrameDomain, NSString *topFrameDomain, NSDictionary<NSString *, NSArray<NSString *> *> *quirkDomains, void  (^completionHandler)(BOOL)) {
        EXPECT_NOT_NULL(subFrameDomain);
        EXPECT_NOT_NULL(subFrameDomain);
        EXPECT_WK_STREQ(@"site2.example", subFrameDomain);
        EXPECT_WK_STREQ(@"site1.example", topFrameDomain);
        EXPECT_NOT_NULL(quirkDomains[topFrameDomain]);
        EXPECT_TRUE([quirkDomains[topFrameDomain] isKindOfClass:[NSArray class]]);
        EXPECT_NE([quirkDomains[topFrameDomain] indexOfObject:subFrameDomain], (NSUInteger) NSNotFound);
        gotRequestStorageAccessPanelForQuirksForDomain = true;
        completionHandler(YES);
    };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/"]]];
    [webView _test_waitForDidFinishNavigation];

    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"let iframeSite2 = document.createElement(\"iframe\"); iframeSite2.src = \"http://site2.example/request-storage-access\"; document.body.appendChild(iframeSite2); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    TestWebKitAPI::Util::run(&gotRequestStorageAccessPanelForQuirksForDomain);
    EXPECT_FALSE(gotRequestStorageAccessPanelForDomain);
    gotRequestStorageAccessPanelForDomain = false;
    gotRequestStorageAccessPanelForQuirksForDomain = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"Granted");

    [webView evaluateJavaScript:@"let iframeSite3 = document.createElement(\"iframe\"); iframeSite3.src = \"http://site3.example/request-storage-access\"; document.body.appendChild(iframeSite3); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    TestWebKitAPI::Util::run(&gotRequestStorageAccessPanelForDomain);
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"Granted");
    EXPECT_FALSE(gotRequestStorageAccessPanelForQuirksForDomain);
    gotRequestStorageAccessPanelForDomain = false;
    gotRequestStorageAccessPanelForQuirksForDomain = false;
}

TEST(ResourceLoadStatistics, StorageAccessGrantMultipleSubFrameDomains)
{
    using namespace TestWebKitAPI;

    auto headerFromRequest = [](const Vector<char>& request, const ASCIILiteral& headerPrefix) {
        StringView requestView(request.span());
        auto headerStart = requestView.find(headerPrefix);
        const auto headerLen = strlen(headerPrefix);
        if (headerStart == notFound)
            return ""_str;
        auto headerValueStart = headerStart + headerLen;
        auto headerEnd = requestView.find("\r\n"_s, headerValueStart);
        if (headerEnd == notFound)
            return ""_str;
        return requestView.substring(headerValueStart, headerEnd - headerValueStart).toStringWithoutCopying();
    };

    bool didSetSite2CookieHeader { false };
    bool didSendSite2CookieHeader { false };
    HTTPServer httpServer { HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();

            URL url { HTTPServer::parsePath(request) };

            if (url.string() == "http://site2.example/set-cookie"_s) {
                co_await connection.awaitableSend(HTTPResponse({ { { "Set-Cookie"_s, "exists=1"_s } }, "<body></body>"_s }).serialize());
                didSetSite2CookieHeader = true;
                continue;
            }

            if (url.string() == "http://site1.example/"_s) {
                co_await connection.awaitableSend(HTTPResponse("<body></body>"_s).serialize());
                continue;
            }

            if ((url.host() == "site2.example"_s || url.host() == "site3.example"_s || url.host() == "site4.example"_s) && url.path() == "/get-cookie"_s) {
                if (url.host() == "site2.example"_s && headerFromRequest(request, "Cookie: "_s) == "exists=1"_s)
                    didSendSite2CookieHeader = true;
                co_await connection.awaitableSend(HTTPResponse("<body><script>alert(\"loaded\");</script>site page 1 iframe</body>"_s).serialize());
                continue;
            }

            if ((url.host() == "site2.example"_s || url.host() == "site3.example"_s || url.host() == "site4.example"_s) && url.path() == "/has-access"_s) {
                co_await connection.awaitableSend(HTTPResponse("<body><script>document.hasStorageAccess().then((allowed) => alert(allowed), (denied) => alert(denied));</script></body>"_s).serialize());
                continue;
            }

            if (url.host() == "site2.example"_s && url.path() == "/redirect"_s) {
                co_await connection.awaitableSend(HTTPResponse({ 302, { { "Location"_s, "http://site3.example/has-access"_s } }, "Redirecting"_s }).serialize());
                continue;
            }

            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::Http };

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [configuration.get() setWebsiteDataStore:dataStore.get()];
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    __block bool done { false };
    [dataStore _clearResourceLoadStatistics:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _logUserInteraction:[NSURL URLWithString:@"http://site2.example/"] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _logUserInteraction:[NSURL URLWithString:@"http://site3.example/"] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [dataStore _setStorageAccessPromptQuirkForTesting:@"site1.example" withSubFrameDomains:[NSArray arrayWithObjects:@"site2.example", @"site3.example", nil] withTriggerPages:@[] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    __block bool gotRequestStorageAccessPanelForQuirksForDomain { false };
    __block NSString *expectedTopFrameDomain;
    __block NSString *expectedSubFrameDomain;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().requestStorageAccessPanelForQuirksForDomain = ^(WKWebView *, NSString *subFrameDomain, NSString *topFrameDomain, NSDictionary<NSString *, NSArray<NSString *> *> *quirkDomains, void  (^completionHandler)(BOOL)) {
        EXPECT_NOT_NULL(expectedSubFrameDomain);
        EXPECT_NOT_NULL(expectedTopFrameDomain);
        EXPECT_NOT_NULL(subFrameDomain);
        EXPECT_NOT_NULL(subFrameDomain);
        EXPECT_WK_STREQ(expectedSubFrameDomain, subFrameDomain);
        EXPECT_WK_STREQ(expectedTopFrameDomain, topFrameDomain);
        EXPECT_NOT_NULL(quirkDomains[topFrameDomain]);
        EXPECT_TRUE([quirkDomains[topFrameDomain] isKindOfClass:[NSArray class]]);
        EXPECT_NE([quirkDomains[topFrameDomain] indexOfObject:subFrameDomain], (NSUInteger) NSNotFound);
        gotRequestStorageAccessPanelForQuirksForDomain = true;
        completionHandler(YES);
    };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site2.example/set-cookie"]]];
    [webView _test_waitForDidFinishNavigation];
    EXPECT_TRUE(didSetSite2CookieHeader);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/"]]];
    [webView _test_waitForDidFinishNavigation];

    [webView setUIDelegate:uiDelegate.get()];

    expectedTopFrameDomain = @"site1.example";
    expectedSubFrameDomain = @"site2.example";
    [webView evaluateJavaScript:@"let iframeSite2 = document.createElement(\"iframe\"); iframeSite2.src = \"http://site2.example/get-cookie\"; document.body.appendChild(iframeSite2); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    TestWebKitAPI::Util::run(&gotRequestStorageAccessPanelForQuirksForDomain);
    gotRequestStorageAccessPanelForQuirksForDomain = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"loaded");
    EXPECT_TRUE(didSendSite2CookieHeader);
    didSendSite2CookieHeader = false;

    [webView evaluateJavaScript:@"iframeSite2.contentWindow.location = \"http://site2.example/has-access\"; true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"true");

    expectedTopFrameDomain = @"site1.example";
    expectedSubFrameDomain = @"site3.example";
    [webView evaluateJavaScript:@"let iframeSite3 = document.createElement(\"iframe\"); iframeSite3.src = \"http://site3.example/get-cookie\"; document.body.appendChild(iframeSite3); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    TestWebKitAPI::Util::run(&gotRequestStorageAccessPanelForQuirksForDomain);
    gotRequestStorageAccessPanelForQuirksForDomain = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"loaded");

    [webView evaluateJavaScript:@"iframeSite3.contentWindow.location = \"http://site3.example/has-access\"; true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"true");

    expectedTopFrameDomain = @"site1.example";
    expectedSubFrameDomain = @"site3.example";
    [webView evaluateJavaScript:@"let iframeSite4 = document.createElement(\"iframe\"); iframeSite4.src = \"http://site4.example/get-cookie\"; document.body.appendChild(iframeSite4); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"loaded");
    EXPECT_FALSE(gotRequestStorageAccessPanelForQuirksForDomain);

    [webView evaluateJavaScript:@"iframeSite4.contentWindow.location = \"http://site4.example/has-access\"; true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"false");

    [webView reload];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"let iframeSite2 = document.createElement(\"iframe\"); iframeSite2.src = \"http://site2.example/redirect\"; document.body.appendChild(iframeSite2); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_WK_STREQ([uiDelegate waitForAlert], @"true");
    EXPECT_FALSE(didSendSite2CookieHeader);
    didSendSite2CookieHeader = false;
}
