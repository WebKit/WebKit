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

#include "config.h"

#import "PlatformUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool finishedNavigation = false;

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

TEST(ResourceLoadStatistics, GrandfatherCallback)
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *fileURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"full_browsing_session_resourceLog.plist"];
    [[NSFileManager defaultManager] removeItemAtURL:fileURL error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:statisticsDirectoryURL error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:statisticsDirectoryURL.path]);

    static bool doneFlag;
    static bool grandfatheredFlag;

    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        ASSERT_TRUE([message isEqualToString:@"Grandfathered"]);
        grandfatheredFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    TestWebKitAPI::Util::run(&grandfatheredFlag);

    // Spin the runloop until the resource load statistics file has written to disk.
    // If the test enters a spin loop here, it has failed.
    while (true) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:fileURL.path])
            break;
        TestWebKitAPI::Util::spinRunLoop(1);
    }

    grandfatheredFlag = false;
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

TEST(ResourceLoadStatistics, ShouldNotGrandfatherOnStartup)
{
    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *targetURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"full_browsing_session_resourceLog.plist"];
    NSURL *testResourceURL = [[NSBundle mainBundle] URLForResource:@"EmptyGrandfatheredResourceLoadStatistics" withExtension:@"plist" subdirectory:@"TestWebKitAPI.resources"];

    [[NSFileManager defaultManager] createDirectoryAtURL:statisticsDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:testResourceURL toURL:targetURL error:nil];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

    static bool callbackFlag;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        ASSERT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        callbackFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    TestWebKitAPI::Util::run(&callbackFlag);
}

TEST(ResourceLoadStatistics, ChildProcessesNotLaunched)
{
    // Ensure the shared process pool exists so the data store operations we're about to do work with it.
    WKProcessPool *sharedProcessPool = [WKProcessPool _sharedProcessPool];

    EXPECT_EQ((size_t)0, [sharedProcessPool _pluginProcessCount]);

    auto *dataStore = [WKWebsiteDataStore defaultDataStore];

    NSURL *statisticsDirectoryURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/WebsiteData/ResourceLoadStatistics" stringByExpandingTildeInPath] isDirectory:YES];
    NSURL *targetURL = [statisticsDirectoryURL URLByAppendingPathComponent:@"full_browsing_session_resourceLog.plist"];
    NSURL *testResourceURL = [[NSBundle mainBundle] URLForResource:@"EmptyGrandfatheredResourceLoadStatistics" withExtension:@"plist" subdirectory:@"TestWebKitAPI.resources"];

    [[NSFileManager defaultManager] createDirectoryAtURL:statisticsDirectoryURL withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:testResourceURL toURL:targetURL error:nil];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:targetURL.path]);

    static bool doneFlag;
    [dataStore _setResourceLoadStatisticsTestingCallback:^(WKWebsiteDataStore *, NSString *message) {
        EXPECT_TRUE([message isEqualToString:@"PopulatedWithoutGrandfathering"]);
        doneFlag = true;
    }];

    // We need an active NetworkProcess to perform ResourceLoadStatistics operations.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    TestWebKitAPI::Util::run(&doneFlag);

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
    WKRetainPtr<WKContextRef> context(AdoptWK, TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
#endif

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[DisableITPDuringNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"notify-resourceLoadObserver" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    TestWebKitAPI::Util::run(&finishedNavigation);
}

#endif

