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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>
#import <wtf/text/WTFString.h>

static bool gotFlag;
static uint64_t observerCallbacks;
static RetainPtr<WKHTTPCookieStore> globalCookieStore;

@interface CookieObserver : NSObject<WKHTTPCookieStoreObserver>
- (void)cookiesDidChangeInCookieStore:(WKHTTPCookieStore *)cookieStore;
@end

@implementation CookieObserver

- (void)cookiesDidChangeInCookieStore:(WKHTTPCookieStore *)cookieStore
{
    ASSERT_EQ(cookieStore, globalCookieStore.get());
    ++observerCallbacks;
}

@end

static void runTestWithWebsiteDataStore(WKWebsiteDataStore* dataStore)
{
    observerCallbacks = 0;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"Oh hello" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:[] {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    // Triggering removeData when we don't have plugin data to remove should not trigger the plugin process to launch.
    id pool = [WKProcessPool _sharedProcessPool];
    EXPECT_EQ([pool _pluginProcessCount], static_cast<size_t>(0));

    globalCookieStore = dataStore.httpCookieStore;
    RetainPtr<CookieObserver> observer1 = adoptNS([[CookieObserver alloc] init]);
    RetainPtr<CookieObserver> observer2 = adoptNS([[CookieObserver alloc] init]);
    [globalCookieStore addObserver:observer1.get()];
    [globalCookieStore addObserver:observer2.get()];

    RetainPtr<NSArray<NSHTTPCookie *>> cookies;
    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);

    ASSERT_EQ([cookies count], 0u);

    gotFlag = false;

    RetainPtr<NSHTTPCookie> cookie1 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"CookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @".www.webkit.org",
        NSHTTPCookieSecure: @"TRUE",
        NSHTTPCookieDiscard: @"TRUE",
        NSHTTPCookieMaximumAge: @"10000",
    }];

    RetainPtr<NSHTTPCookie> cookie2 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/path",
        NSHTTPCookieName: @"OtherCookieName",
        NSHTTPCookieValue: @"OtherCookieValue",
        NSHTTPCookieDomain: @".www.w3c.org",
        NSHTTPCookieMaximumAge: @"10000",
    }];

    [globalCookieStore setCookie:cookie1.get() completionHandler:[](){
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore setCookie:cookie2.get() completionHandler:[](){
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    ASSERT_EQ([cookies count], 2u);
    while (observerCallbacks != 4u)
        TestWebKitAPI::Util::spinRunLoop();

    for (NSHTTPCookie *cookie : cookies.get()) {
        if ([cookie.name isEqual:@"CookieName"]) {
            ASSERT_TRUE([cookie1.get().path isEqualToString:cookie.path]);
            ASSERT_TRUE([cookie1.get().value isEqualToString:cookie.value]);
            ASSERT_TRUE([cookie1.get().domain isEqualToString:cookie.domain]);
            ASSERT_TRUE(cookie.secure);
            ASSERT_TRUE(cookie.sessionOnly);
        } else {
            ASSERT_TRUE([cookie2.get().path isEqualToString:cookie.path]);
            ASSERT_TRUE([cookie2.get().value isEqualToString:cookie.value]);
            ASSERT_TRUE([cookie2.get().name isEqualToString:cookie.name]);
            ASSERT_TRUE([cookie2.get().domain isEqualToString:cookie.domain]);
            ASSERT_FALSE(cookie.secure);
            ASSERT_FALSE(cookie.sessionOnly);
        }
    }

    [globalCookieStore deleteCookie:cookie2.get() completionHandler:[](){
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    ASSERT_EQ([cookies count], 1u);
    while (observerCallbacks != 6u)
        TestWebKitAPI::Util::spinRunLoop();

    for (NSHTTPCookie *cookie : cookies.get()) {
        ASSERT_TRUE([cookie1.get().path isEqualToString:cookie.path]);
        ASSERT_TRUE([cookie1.get().value isEqualToString:cookie.value]);
        ASSERT_TRUE([cookie1.get().domain isEqualToString:cookie.domain]);
        ASSERT_TRUE(cookie.secure);
        ASSERT_TRUE(cookie.sessionOnly);
    }

    [globalCookieStore removeObserver:observer1.get()];
    [globalCookieStore removeObserver:observer2.get()];
}

TEST(WKHTTPCookieStore, Basic)
{
    runTestWithWebsiteDataStore([WKWebsiteDataStore defaultDataStore]);
}

TEST(WKHTTPCookieStore, ProcessPrivilege)
{
    // Make sure UI process has no privilege at the beginning.
    WTF::setProcessPrivileges({ });

    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:[] {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    globalCookieStore = [[WKWebsiteDataStore defaultDataStore] httpCookieStore];

    RetainPtr<NSHTTPCookie> cookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"Cookie",
        NSHTTPCookieValue: @"Value",
        NSHTTPCookieDomain: @".www.webkit.org",
    }];

    [globalCookieStore setCookie:cookie.get() completionHandler:[]() {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;


    [globalCookieStore getAllCookies:^(NSArray<NSHTTPCookie *>*cookies) {
        ASSERT_EQ(1u, cookies.count);
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;
}

TEST(WKHTTPCookieStore, HttpOnly)
{
    WKWebsiteDataStore* dataStore = [WKWebsiteDataStore defaultDataStore];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:[] {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    globalCookieStore = dataStore.httpCookieStore;

    RetainPtr<NSArray<NSHTTPCookie *>> cookies;
    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;
    ASSERT_EQ([cookies count], 0u);

    auto cookieProperties = adoptNS([[NSMutableDictionary alloc] init]);
    [cookieProperties setObject:@"cookieName" forKey:NSHTTPCookieName];
    [cookieProperties setObject:@"cookieValue" forKey:NSHTTPCookieValue];
    [cookieProperties setObject:@".www.webkit.org" forKey:NSHTTPCookieDomain];
    [cookieProperties setObject:@"/path" forKey:NSHTTPCookiePath];
    [cookieProperties setObject:@YES forKey:@"HttpOnly"];
    RetainPtr<NSHTTPCookie> httpOnlyCookie = [NSHTTPCookie cookieWithProperties:cookieProperties.get()];
    [cookieProperties setObject:@"cookieValue2" forKey:NSHTTPCookieValue];
    RetainPtr<NSHTTPCookie> httpOnlyCookie2 = [NSHTTPCookie cookieWithProperties:cookieProperties.get()];
    [cookieProperties removeObjectForKey:@"HttpOnly"];
    RetainPtr<NSHTTPCookie> notHttpOnlyCookie = [NSHTTPCookie cookieWithProperties:cookieProperties.get()];

    EXPECT_TRUE(httpOnlyCookie.get().HTTPOnly);
    EXPECT_FALSE(notHttpOnlyCookie.get().HTTPOnly);

    // Setting httpOnlyCookie should succeed.
    [globalCookieStore setCookie:httpOnlyCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    // Setting httpOnlyCookie2 should succeed.
    [globalCookieStore setCookie:httpOnlyCookie2.get() completionHandler:[]() {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;
    ASSERT_EQ([cookies count], 1u);
    EXPECT_TRUE([[[cookies objectAtIndex:0] value] isEqual:@"cookieValue2"]);

    // Setting notHttpOnlyCookie should fail because we cannot overwrite HTTPOnly property using public API.
    [globalCookieStore setCookie:notHttpOnlyCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;
    ASSERT_EQ([cookies count], 1u);
    EXPECT_TRUE([[cookies objectAtIndex:0] isHTTPOnly]);

    // Deleting notHttpOnlyCookie should fail because the cookie stored is HTTPOnly.
    [globalCookieStore deleteCookie:notHttpOnlyCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;
    ASSERT_EQ([cookies count], 1u);

    // Deleting httpOnlyCookie should succeed. 
    [globalCookieStore deleteCookie:httpOnlyCookie.get() completionHandler:[]() {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *nsCookies) {
        cookies = nsCookies;
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;
    ASSERT_EQ([cookies count], 0u);
}

// FIXME: Would be good to enable this test for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
TEST(WKHTTPCookieStore, CreationTime)
{   
    auto dataStore = [WKWebsiteDataStore defaultDataStore];
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"WebKit Test" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:[] {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    globalCookieStore = dataStore.httpCookieStore;

    auto cookieProperties = adoptNS([[NSMutableDictionary alloc] init]);
    [cookieProperties setObject:@"cookieName" forKey:NSHTTPCookieName];
    [cookieProperties setObject:@"cookieValue" forKey:NSHTTPCookieValue];
    [cookieProperties setObject:@".www.webkit.org" forKey:NSHTTPCookieDomain];
    [cookieProperties setObject:@"/path" forKey:NSHTTPCookiePath];
    RetainPtr<NSNumber> creationTime = @(100000);
    [cookieProperties setObject:creationTime.get() forKey:@"Created"];
    RetainPtr<NSHTTPCookie> cookie = [NSHTTPCookie cookieWithProperties:cookieProperties.get()];

    [globalCookieStore setCookie:cookie.get() completionHandler:[]() {
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    [globalCookieStore getAllCookies:[&](NSArray<NSHTTPCookie *> *cookies) {
        ASSERT_EQ(1u, cookies.count);
        NSNumber* createdTime = [cookies objectAtIndex:0].properties[@"Created"];
        EXPECT_TRUE([creationTime.get() isEqual:createdTime]);
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;

    sleep(1_s);

    [globalCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
        ASSERT_EQ(1u, cookies.count);
        NSNumber* createdTime = [cookies objectAtIndex:0].properties[@"Created"];
        EXPECT_TRUE([creationTime.get() isEqual:createdTime]);
        gotFlag = true;
    }];
    TestWebKitAPI::Util::run(&gotFlag);
    gotFlag = false;
}
#endif

// FIXME: This #if should be removed once <rdar://problem/35344202> is resolved and bots are updated.
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED <= 101301
TEST(WKHTTPCookieStore, NonPersistent)
{
    RetainPtr<WKWebsiteDataStore> nonPersistentDataStore;
    @autoreleasepool {
        nonPersistentDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    }

    runTestWithWebsiteDataStore(nonPersistentDataStore.get());
}

TEST(WKHTTPCookieStore, Custom)
{
    NSURL *cookieStorageFile = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/CookieStorage/Cookie.File" stringByExpandingTildeInPath] isDirectory:NO];
    NSURL *idbPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];

    [[NSFileManager defaultManager] removeItemAtURL:cookieStorageFile error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:idbPath error:nil];

    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:cookieStorageFile.path]);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = idbPath;
    websiteDataStoreConfiguration.get()._cookieStorageFile = cookieStorageFile;

    auto customDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    runTestWithWebsiteDataStore(customDataStore.get());
}
#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED <= 101301

TEST(WebKit, CookieObserverCrash)
{
    RetainPtr<WKWebsiteDataStore> nonPersistentDataStore;
    @autoreleasepool {
        nonPersistentDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    }

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = nonPersistentDataStore.get();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:@"Oh hello" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView _test_waitForDidFinishNavigation];

    globalCookieStore = nonPersistentDataStore.get().httpCookieStore;
    RetainPtr<CookieObserver> observer = adoptNS([[CookieObserver alloc] init]);
    [globalCookieStore addObserver:observer.get()];

    [globalCookieStore getAllCookies:[](NSArray<NSHTTPCookie *> *) {
        gotFlag = true;
    }];

    TestWebKitAPI::Util::run(&gotFlag);
}

static void clearCookies(WKWebsiteDataStore *dataStore)
{
    __block bool deleted = false;
    [dataStore removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeCookies] modifiedSince:[NSDate distantPast] completionHandler:^{
        deleted = true;
    }];
    TestWebKitAPI::Util::run(&deleted);
}

TEST(WKHTTPCookieStore, ObserveCookiesReceivedFromHTTP)
{
    TestWebKitAPI::HTTPServer server({{ "/"_s, {{{ "Set-Cookie"_s, "testkey=testvalue"_s }}, "hello"_s }}});

    auto runTest = [&] (WKWebsiteDataStore *dataStore) {
        auto configuration = adoptNS([WKWebViewConfiguration new]);
        configuration.get().websiteDataStore = dataStore;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto observer = adoptNS([CookieObserver new]);
        globalCookieStore = webView.get().configuration.websiteDataStore.httpCookieStore;
        clearCookies(dataStore);
        [globalCookieStore addObserver:observer.get()];
        observerCallbacks = 0;
        [webView loadRequest:server.request()];
        [webView _test_waitForDidFinishNavigation];
        while (!observerCallbacks)
            TestWebKitAPI::Util::spinRunLoop();
        __block bool gotCookie = false;
        [globalCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
            RELEASE_ASSERT(cookies.count == 1u);
            EXPECT_WK_STREQ(cookies[0].name, "testkey");
            EXPECT_WK_STREQ(cookies[0].value, "testvalue");
            gotCookie = true;
        }];
        TestWebKitAPI::Util::run(&gotCookie);
        EXPECT_EQ(observerCallbacks, 1u);
    };

    runTest([WKWebsiteDataStore defaultDataStore]);
    runTest([WKWebsiteDataStore nonPersistentDataStore]);
}

static bool finished;

@interface CookieUIDelegate : NSObject <WKUIDelegate>
@end

@implementation CookieUIDelegate
- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_STREQ("PersistentCookieName=CookieValue; SessionCookieName=CookieValue", message.UTF8String);
    finished = true;
    completionHandler();
}
@end

enum class ShouldEnableProcessPrewarming : bool { No, Yes };
void runWKHTTPCookieStoreWithoutProcessPool(ShouldEnableProcessPrewarming shouldEnableProcessPrewarming)
{
    RetainPtr<NSHTTPCookie> sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"SessionCookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"127.0.0.1",
    }];
    RetainPtr<NSHTTPCookie> persistentCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"PersistentCookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"127.0.0.1",
        NSHTTPCookieExpires: [NSDate distantFuture],
    }];
    NSString *alertCookieHTML = @"<script>var cookies = document.cookie.split(';'); for (let i = 0; i < cookies.length; i ++) { cookies[i] = cookies[i].trim(); } cookies.sort(); alert(cookies.join('; '));</script>";

    // NonPersistentDataStore
    RetainPtr<WKWebsiteDataStore> ephemeralStoreWithCookies = [WKWebsiteDataStore nonPersistentDataStore];

    finished = false;
    [ephemeralStoreWithCookies.get().httpCookieStore setCookie:persistentCookie.get() completionHandler:^{
        WKWebsiteDataStore *ephemeralStoreWithIndependentCookieStorage = [WKWebsiteDataStore nonPersistentDataStore];
        [ephemeralStoreWithIndependentCookieStorage.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
            ASSERT_EQ(0u, cookies.count);
            finished = true;
        }];
    }];
    TestWebKitAPI::Util::run(&finished);

    finished = false;
    [ephemeralStoreWithCookies.get().httpCookieStore setCookie:sessionCookie.get() completionHandler:^{
        [ephemeralStoreWithCookies.get().httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
            ASSERT_EQ(2u, cookies.count);
            finished = true;
        }];
    }];
    TestWebKitAPI::Util::run(&finished);
    finished = false;

    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().prewarmsProcessesAutomatically = shouldEnableProcessPrewarming == ShouldEnableProcessPrewarming::Yes;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = ephemeralStoreWithCookies.get();
    [configuration setProcessPool:processPool.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[CookieUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();
    [webView loadHTMLString:alertCookieHTML baseURL:[NSURL URLWithString:@"http://127.0.0.1"]];
    TestWebKitAPI::Util::run(&finished);

    finished = false;
    [ephemeralStoreWithCookies.get().httpCookieStore deleteCookie:sessionCookie.get() completionHandler:^{
        [ephemeralStoreWithCookies.get().httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
            ASSERT_EQ(1u, cookies.count);
            finished = true;
        }];
    }];
    TestWebKitAPI::Util::run(&finished);
    
    // DefaultDataStore
    auto defaultStore = [WKWebsiteDataStore defaultDataStore];
    finished = false;
    [defaultStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:[] {
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);

    finished = false;
    [defaultStore.httpCookieStore setCookie:persistentCookie.get() completionHandler:^{
        [defaultStore.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
            ASSERT_EQ(1u, cookies.count);
            finished = true;
        }];
    }];
    TestWebKitAPI::Util::run(&finished);

    finished = false;
    [defaultStore.httpCookieStore setCookie:sessionCookie.get() completionHandler:^{
        [defaultStore.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
            ASSERT_EQ(2u, cookies.count);
            finished = true;
        }];
    }];
    TestWebKitAPI::Util::run(&finished);

    finished = false;
    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = defaultStore;
    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().UIDelegate = delegate.get();
    [webView loadHTMLString:alertCookieHTML baseURL:[NSURL URLWithString:@"http://127.0.0.1"]];
    TestWebKitAPI::Util::run(&finished);
}

TEST(WKHTTPCookieStore, WithoutProcessPoolWithoutPrewarming)
{
    runWKHTTPCookieStoreWithoutProcessPool(ShouldEnableProcessPrewarming::No);
}

TEST(WKHTTPCookieStore, WithoutProcessPoolWithPrewarming)
{
    runWKHTTPCookieStoreWithoutProcessPool(ShouldEnableProcessPrewarming::Yes);
}

@interface CheckSessionCookieUIDelegate : NSObject <WKUIDelegate>
- (NSString *)alertCookieHTML;
- (NSString *)waitForMessage;
@end

@implementation CheckSessionCookieUIDelegate {
    RetainPtr<NSString> _message;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    _message = message;
    finished = true;
    completionHandler();
}

- (NSString *)waitForMessage
{
    while (!_message)
        TestWebKitAPI::Util::spinRunLoop();
    return _message.autorelease();
}

- (NSString *)alertCookieHTML
{
    return @"<script>var cookies = document.cookie.split(';'); for (let i = 0; i < cookies.length; i ++) { cookies[i] = cookies[i].trim(); } cookies.sort(); alert(cookies.join('; '));</script>";
}

@end

TEST(WebKit, WKHTTPCookieStoreMultipleViews)
{
    auto webView1 = adoptNS([TestWKWebView new]);
    [webView1 synchronouslyLoadHTMLString:@"start network process"];

    NSHTTPCookie *sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"SessionCookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"127.0.0.1",
    }];

    __block bool setCookieDone = false;
    [[webView1 configuration].websiteDataStore.httpCookieStore setCookie:sessionCookie completionHandler:^{
        setCookieDone = true;
    }];
    TestWebKitAPI::Util::run(&setCookieDone);

    auto delegate = adoptNS([CheckSessionCookieUIDelegate new]);
    [webView1 setUIDelegate:delegate.get()];

    [webView1 loadHTMLString:[delegate alertCookieHTML] baseURL:[NSURL URLWithString:@"http://127.0.0.1"]];
    EXPECT_WK_STREQ([delegate waitForMessage], "SessionCookieName=CookieValue");

    auto webView2 = adoptNS([TestWKWebView new]);
    [webView2 setUIDelegate:delegate.get()];
    [webView2 loadHTMLString:[delegate alertCookieHTML] baseURL:[NSURL URLWithString:@"http://127.0.0.1"]];
    EXPECT_WK_STREQ([delegate waitForMessage], "SessionCookieName=CookieValue");
}

TEST(WKHTTPCookieStore, WithoutProcessPoolEphemeralSession)
{
    RetainPtr<WKWebsiteDataStore> ephemeralStoreWithCookies = [WKWebsiteDataStore nonPersistentDataStore];
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = ephemeralStoreWithCookies.get();
    
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[CheckSessionCookieUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();
    
    RetainPtr<NSHTTPCookie> sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"SessionCookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"127.0.0.1",
    }];
    
    [ephemeralStoreWithCookies.get().httpCookieStore setCookie:sessionCookie.get() completionHandler:^{
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    finished = false;
    
    [webView loadHTMLString:[delegate alertCookieHTML] baseURL:[NSURL URLWithString:@"http://127.0.0.1"]];
    EXPECT_WK_STREQ([delegate waitForMessage], "SessionCookieName=CookieValue");
}

static bool areCookiesEqual(NSHTTPCookie *first, NSHTTPCookie *second)
{
    return [first.name isEqual:second.name] && [first.domain isEqual:second.domain] && [first.path isEqual:second.path] && [first.value isEqual:second.value];
}

TEST(WKHTTPCookieStore, WithoutProcessPoolDuplicates)
{
    RetainPtr<WKHTTPCookieStore> httpCookieStore = [WKWebsiteDataStore defaultDataStore].httpCookieStore;
    RetainPtr<NSHTTPCookie> sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"SessionCookieName",
        NSHTTPCookieValue: @"CookieValue",
        NSHTTPCookieDomain: @"127.0.0.1",
    }];

    auto properties = adoptNS([sessionCookie.get().properties mutableCopy]);
    properties.get()[NSHTTPCookieDomain] = @"localhost";
    RetainPtr<NSHTTPCookie> sessionCookieDifferentDomain = [NSHTTPCookie cookieWithProperties:properties.get()];
    properties.get()[NSHTTPCookieValue] = @"OtherCookieValue";
    RetainPtr<NSHTTPCookie> sessionCookieDifferentValue = [NSHTTPCookie cookieWithProperties:properties.get()];
    finished = false;

    clearCookies([WKWebsiteDataStore defaultDataStore]);

    [httpCookieStore.get() setCookie:sessionCookie.get() completionHandler:^{
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    finished = false;

    [httpCookieStore.get() setCookie:sessionCookieDifferentDomain.get() completionHandler:^{
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    finished = false;

    [httpCookieStore.get() setCookie:sessionCookieDifferentValue.get() completionHandler:^{
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    finished = false;

    [httpCookieStore.get() getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
        EXPECT_EQ(2u, cookies.count);
        bool sessionCookieExists = false, otherSessionCookieExists = false;
        for (NSHTTPCookie* cookie in cookies) {
            if (areCookiesEqual(cookie, sessionCookie.get()))
                sessionCookieExists = true;
            else if (areCookiesEqual(cookie, sessionCookieDifferentValue.get()))
                otherSessionCookieExists = true;
        }
        EXPECT_TRUE(sessionCookieExists && otherSessionCookieExists);
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
}

TEST(WKHTTPCookieStore, CookiesForURL)
{
    WKHTTPCookieStore *cookieStore = [WKWebsiteDataStore defaultDataStore].httpCookieStore;
    NSHTTPCookie *webKitCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"webKitName",
        NSHTTPCookieValue: @"webKitValue",
        NSHTTPCookieDomain: @"webkit.org",
    }];
    NSHTTPCookie *albuquerque = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"appleName",
        NSHTTPCookieValue: @"appleValue",
        NSHTTPCookieDomain: @"apple.org",
    }];

    __block bool done = false;
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"start network process"];
    [cookieStore setCookie:webKitCookie completionHandler:^{
        [cookieStore setCookie:albuquerque completionHandler:^{
            [cookieStore _getCookiesForURL:[NSURL URLWithString:@"https://webkit.org/"] completionHandler:^(NSArray<NSHTTPCookie *> *cookies) {
                EXPECT_EQ(cookies.count, 1ull);
                EXPECT_WK_STREQ(cookies[0].name, "webKitName");
                [cookieStore deleteCookie:webKitCookie completionHandler:^{
                    [cookieStore deleteCookie:albuquerque completionHandler:^{
                        done = true;
                    }];
                }];
            }];
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKHTTPCookieStore, CookieAccessAfterNetworkProcessTermination)
{
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"start network process" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    kill([WKWebsiteDataStore.defaultDataStore _networkProcessIdentifier], SIGKILL);
    TestWebKitAPI::Util::runFor(Seconds(0.1));
    [webView stringByEvaluatingJavaScript:@"document.cookie = 'key=value'"];
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.cookie"], "key=value");
}

TEST(WKHTTPCookieStore, WebSocketCookies)
{
    using namespace TestWebKitAPI;
    bool receivedThirdRequest { false };
    uint16_t serverPort { 0 };
    HTTPServer server(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> Task { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        auto path = HTTPServer::parsePath(request);
        request.append(0);
        if (path == "/com"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 0\r\n"
                "Set-Cookie: Default=1\r\n"
                "Set-Cookie: SameSite_None=1; SameSite=None\r\n"
                "Set-Cookie: SameSite_None_Secure=1; secure; SameSite=None\r\n"
                "Set-Cookie: SameSite_Lax=1; SameSite=Lax\r\n"
                "Set-Cookie: SameSite_Strict=1; SameSite=Strict\r\n"
                "\r\n"_s);
        } else if (path == "/websocket"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: 127.0.0.1:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            receivedThirdRequest = true;
        } else if (path == "/ninja"_s) {
            auto html = [NSString stringWithFormat:@"<script>new WebSocket('ws://127.0.0.1:%d/websocket')</script>", serverPort];
            co_await connection.awaitableSend(HTTPResponse(html).serialize());
        } else
            EXPECT_WK_STREQ(@"SHOULD NOT BE REACH", path);
    } });
    serverPort = server.port();

    auto webView = adoptNS([WKWebView new]);
    [[[webView configuration] websiteDataStore] _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/com", serverPort]]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://localhost:%d/ninja", serverPort]]]];
    Util::run(&receivedThirdRequest);
}

TEST(WKHTTPCookieStore, WebSocketCookiesFromRedirect)
{
    using namespace TestWebKitAPI;
    bool receivedWebSocket { false };
    uint16_t serverPort { 0 };
    HTTPServer server(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> Task { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        auto path = HTTPServer::parsePath(request);
        request.append(0);
        if (path == "/redirect"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: http://localhost:"_s + serverPort + "/com\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/com"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: http://127.0.0.1:"_s + serverPort + "/destination\r\n"_s
                "Set-Cookie: Default=1\r\n"
                "Set-Cookie: SameSite_None=1; SameSite=None\r\n"
                "Set-Cookie: SameSite_None_Secure=1; secure; SameSite=None\r\n"
                "Set-Cookie: SameSite_Lax=1; SameSite=Lax\r\n"
                "Set-Cookie: SameSite_Strict=1; SameSite=Strict\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/destination"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: 127.0.0.1:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            co_await connection.awaitableSend(
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/websocket"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: localhost:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            EXPECT_TRUE(strnstr(request.data(), "Origin: http://127.0.0.1:", request.size()));
            receivedWebSocket = true;
        } else if (path == "/ninja"_s) {
            auto html = [NSString stringWithFormat:@"<script>new WebSocket('ws://localhost:%d/websocket')</script>", serverPort];
            co_await connection.awaitableSend(HTTPResponse(html).serialize());
        } else
            EXPECT_WK_STREQ(@"SHOULD NOT BE REACH", path);
    } });
    serverPort = server.port();

    auto webView = adoptNS([WKWebView new]);
    [[[webView configuration] websiteDataStore] _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/redirect", serverPort]]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/ninja", serverPort]]]];
    Util::run(&receivedWebSocket);
}

TEST(WKHTTPCookieStore, WebSocketCookiesThroughRedirect)
{
    using namespace TestWebKitAPI;
    bool receivedWebSocket { false };
    uint16_t serverPort { 0 };
    HTTPServer server(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> Task { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        auto path = HTTPServer::parsePath(request);
        request.append(0);
        if (path == "/redirect"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: http://localhost:"_s + serverPort + "/com\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/com"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: http://127.0.0.1:"_s + serverPort + "/ninja\r\n"_s
                "Set-Cookie: Default=1\r\n"
                "Set-Cookie: SameSite_None=1; SameSite=None\r\n"
                "Set-Cookie: SameSite_None_Secure=1; secure; SameSite=None\r\n"
                "Set-Cookie: SameSite_Lax=1; SameSite=Lax\r\n"
                "Set-Cookie: SameSite_Strict=1; SameSite=Strict\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/websocket"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: localhost:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            EXPECT_TRUE(strnstr(request.data(), "Origin: http://127.0.0.1:", request.size()));
            receivedWebSocket = true;
        } else if (path == "/ninja"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: 127.0.0.1:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            auto html = [NSString stringWithFormat:@"<script>new WebSocket('ws://localhost:%d/websocket')</script>", serverPort];
            co_await connection.awaitableSend(HTTPResponse(html).serialize());
        } else
            EXPECT_WK_STREQ(@"SHOULD NOT BE REACH", path);
    } });
    serverPort = server.port();

    auto webView = adoptNS([WKWebView new]);
    [[[webView configuration] websiteDataStore] _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/redirect", serverPort]]]];
    Util::run(&receivedWebSocket);
}

TEST(WKHTTPCookieStore, WebSocketSetCookiesThroughFirstPartyRedirect)
{
    using namespace TestWebKitAPI;
    bool receivedWebSocket { false };
    uint16_t serverPort { 0 };
    HTTPServer server(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> Task { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        auto path = HTTPServer::parsePath(request);
        request.append(0);
        if (path == "/redirect"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: ws://localhost:"_s + serverPort + "/websocket\r\n"
                "Set-Cookie: Default=1\r\n"
                "Set-Cookie: SameSite_None=1; SameSite=None\r\n"
                "Set-Cookie: SameSite_None_Secure=1; secure; SameSite=None\r\n"
                "Set-Cookie: SameSite_Lax=1; SameSite=Lax\r\n"
                "Set-Cookie: SameSite_Strict=1; SameSite=Strict\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/websocket"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: localhost:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Origin: http://127.0.0.1:", request.size()));
            receivedWebSocket = true;
        } else if (path == "/ninja"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: 127.0.0.1:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            auto html = [NSString stringWithFormat:@"<script>new WebSocket('ws://127.0.0.1:%d/redirect')</script>", serverPort];
            co_await connection.awaitableSend(HTTPResponse(html).serialize());
        } else
            EXPECT_WK_STREQ(@"SHOULD NOT BE REACH", path);
    } });
    serverPort = server.port();

    auto webView = adoptNS([WKWebView new]);
    [[[webView configuration] websiteDataStore] _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/ninja", serverPort]]]];
    Util::run(&receivedWebSocket);
}

TEST(WKHTTPCookieStore, WebSocketSetCookiesThroughRedirectToThirdParty)
{
    using namespace TestWebKitAPI;
    bool receivedWebSocket { false };
    uint16_t serverPort { 0 };
    HTTPServer server(TestWebKitAPI::HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> Task { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        auto path = HTTPServer::parsePath(request);
        request.append(0);
        if (path == "/redirect"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: ws://localhost:"_s + serverPort + "/com\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/com"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: ws://127.0.0.1:"_s + serverPort + "/redirect2\r\n"_s
                "Set-Cookie: Default=1\r\n"
                "Set-Cookie: SameSite_None=1; SameSite=None\r\n"
                "Set-Cookie: SameSite_None_Secure=1; secure; SameSite=None\r\n"
                "Set-Cookie: SameSite_Lax=1; SameSite=Lax\r\n"
                "Set-Cookie: SameSite_Strict=1; SameSite=Strict\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/redirect2"_s) {
            co_await connection.awaitableSend(
                "HTTP/1.1 302 Found\r\n"
                "Location: ws://localhost:"_s + serverPort + "/websocket\r\n"
                "Content-Length: 0\r\n"
                "\r\n"_s);
        } else if (path == "/websocket"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: localhost:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            receivedWebSocket = true;
        } else if (path == "/ninja"_s) {
            EXPECT_TRUE(strnstr(request.data(), "Host: 127.0.0.1:", request.size()));
            EXPECT_FALSE(strnstr(request.data(), "Cookie:", request.size()));
            auto html = [NSString stringWithFormat:@"<script>new WebSocket('ws://127.0.0.1:%d/redirect')</script>", serverPort];
            co_await connection.awaitableSend(HTTPResponse(html).serialize());
        } else
            EXPECT_WK_STREQ(@"SHOULD NOT BE REACH", path);
    } });
    serverPort = server.port();

    auto webView = adoptNS([WKWebView new]);
    [[[webView configuration] websiteDataStore] _setResourceLoadStatisticsEnabled:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/ninja", serverPort]]]];
    Util::run(&receivedWebSocket);
}

TEST(WKHTTPCookieStore, SameSiteWithPatternMatch)
{
    using namespace TestWebKitAPI;
    HTTPServer httpServer { HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> Task {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            request.append(0);
            if (path == "http://site1.example/set-cookie"_s || path == "http://site2.example/set-cookie"_s) {
                co_await connection.awaitableSend(HTTPResponse({ { { "Set-Cookie"_s, "exists=1;samesite=strict"_s } }, "<body></body>"_s }).serialize());
                continue;
            }
            StringView requestView(request.span());
            if (path == "http://site1.example/get-cookie"_s)
                EXPECT_EQ(requestView.find("Cookie: exists=1"_s), notFound);
            else if (path == "http://site2.example/get-cookie"_s)
                EXPECT_NE(requestView.find("Cookie: exists=1"_s), notFound);
            else
                EXPECT_WK_STREQ(@"SHOULD NOT BE REACH", path);
            co_await connection.awaitableSend(HTTPResponse("<body></body>"_s).serialize());
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
    [configuration.get() _setCORSDisablingPatterns:@[@"http://site2.example/*"]];
    configuration.get()._shouldRelaxThirdPartyCookieBlocking = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site1.example/set-cookie"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site2.example/set-cookie"]]];
    [webView _test_waitForDidFinishNavigation];

    [webView loadHTMLString:@"<body>body</body>" baseURL:[NSURL URLWithString:@"http://site3.example"]];
    [webView _test_waitForDidFinishNavigation];

    __block bool doneEvaluatingJavaScript { false };
    [webView evaluateJavaScript:@"fetch(\"http://site1.example/get-cookie\").then(() => alert(\"Fetched\")).catch(() => alert(\"Failed\")); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_WK_STREQ(error.description, @"");
        doneEvaluatingJavaScript = true;
    }];
    Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ([webView _test_waitForAlert], @"Failed");
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"fetch(\"http://site2.example/get-cookie\").then(() => alert(\"Fetched\")).catch(() => alert(\"Failed\")); true" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_WK_STREQ(error.description, @"");
        doneEvaluatingJavaScript = true;
    }];
    Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ([webView _test_waitForAlert], @"Fetched");
    doneEvaluatingJavaScript = false;
}
