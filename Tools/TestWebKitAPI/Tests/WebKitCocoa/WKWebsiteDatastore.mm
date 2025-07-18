/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataSize.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

static RetainPtr<NSURLCredential> persistentCredential;
static bool usePersistentCredentialStorage = false;

@interface NavigationTestDelegate : NSObject <WKNavigationDelegate>
@end

@implementation NavigationTestDelegate {
    bool _hasFinishedNavigation;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    _hasFinishedNavigation = false;
    
    return self;
}

- (void)waitForDidFinishNavigation
{
    TestWebKitAPI::Util::run(&_hasFinishedNavigation);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _hasFinishedNavigation = true;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    static bool firstChallenge = true;
    if (firstChallenge) {
        firstChallenge = false;
        persistentCredential = adoptNS([[NSURLCredential alloc] initWithUser:@"username" password:@"password" persistence:(usePersistentCredentialStorage ? NSURLCredentialPersistencePermanent: NSURLCredentialPersistenceForSession)]);
        return completionHandler(NSURLSessionAuthChallengeUseCredential, persistentCredential.get());
    }
    return completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}
@end

@interface WKWebsiteDataStoreMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation WKWebsiteDataStoreMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

namespace TestWebKitAPI {

TEST(WKWebsiteDataStore, RemoveAndFetchData)
{
    RetainPtr defaultDataStore = [WKWebsiteDataStore defaultDataStore];
    readyToContinue = false;
    [defaultDataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    
    readyToContinue = false;
    [defaultDataStore fetchDataRecordsOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0u, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, RemoveEphemeralData)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    __block bool done = false;
    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler: ^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

static constexpr auto mainBytes = R"SWRESOURCE(
<script>
function log(message)
{
    window.webkit.messageHandlers.testHandler.postMessage(message);
}

function installServiceWorker()
{
    navigator.serviceWorker.register('/sw.js').then((registration) => {
        const worker = registration.installing ? registration.installing : registration.active;
        worker.postMessage('Hello');
    }).catch((error) => {
        log('register() failed with: ' + error);
    });
}

navigator.serviceWorker.addEventListener('message', function(event) {
    log('Message from ServiceWorker: ' + event.data);
});

installServiceWorker();
</script>
)SWRESOURCE"_s;

static constexpr auto scriptBytes = R"SWRESOURCE(
async function cacheResources(resources)
{
    const cache = await caches.open("v1");
    await cache.addAll(resources);
}

self.addEventListener('message', (event) => {
    event.source.postMessage(event.data);
});

self.addEventListener('install', (event) => {
    event.waitUntil(cacheResources(["/cached_1.html", "/cached_2.html", "/cached_3.html", "/cached_4.html", "/cached_5.html"]));
});
)SWRESOURCE"_s;

TEST(WKWebsiteDataStore, RemoveDataWaitUntilWebProcessesExit)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
        { "/cached_1.html"_s, { "hi"_s } },
        { "/cached_2.html"_s, { "hi"_s } },
        { "/cached_3.html"_s, { "hi"_s } },
        { "/cached_4.html"_s, { "hi"_s } },
        { "/cached_5.html"_s, { "hi"_s } }
    });
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    RetainPtr handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    receivedScriptMessage = false;
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Message from ServiceWorker: Hello", [lastScriptMessage body]);

    // Service worker process may keep requesting resources after page closes.
    [webView _close];
    webView = nullptr;

    // Service worker process will be shut down.
    readyToContinue = false;
    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler: ^{
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[configuration websiteDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(dataRecords.count, 0u);
        for (WKWebsiteDataRecord* record in dataRecords) {
            for (NSString *type in record.dataTypes)
                NSLog(@"Unexpected record with type: [%@]", type);
        }
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    // Ensure service worker can be installed again.
    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    receivedScriptMessage = false;
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Message from ServiceWorker: Hello", [lastScriptMessage body]);
}

TEST(WKWebsiteDataStore, FetchNonPersistentCredentials)
{
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);
    
    usePersistentCredentialStorage = false;
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration setWebsiteDataStore:websiteDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_EQ(credentialCount, 1);
        for (WKWebsiteDataRecord *record in dataRecords)
            ASSERT_TRUE([[record displayName] isEqualToString:@"127.0.0.1"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, FetchPersistentCredentials)
{
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);

    usePersistentCredentialStorage = true;
    auto websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    // Make sure no credential left by previous tests.
    auto protectionSpace = adoptNS([[NSURLProtectionSpace alloc] initWithHost:@"127.0.0.1" port:server.port() protocol:NSURLProtectionSpaceHTTP realm:@"testrealm" authenticationMethod:NSURLAuthenticationMethodHTTPBasic]);
    [[webView configuration].processPool _clearPermanentCredentialsForProtectionSpace:protectionSpace.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        EXPECT_EQ(credentialCount, 0);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Clear persistent credentials created by this test.
    [[webView configuration].processPool _clearPermanentCredentialsForProtectionSpace:protectionSpace.get()];
}

TEST(WKWebsiteDataStore, RemoveNonPersistentCredentials)
{
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);

    usePersistentCredentialStorage = false;
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration setWebsiteDataStore:websiteDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    __block RetainPtr<WKWebsiteDataRecord> expectedRecord;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_GT(credentialCount, 0);
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                expectedRecord = record;
                break;
            }
        }
        EXPECT_TRUE(expectedRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] forDataRecords:@[expectedRecord.get()] completionHandler:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        bool foundLocalHostRecord = false;
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                foundLocalHostRecord = true;
                break;
            }
        }
        EXPECT_FALSE(foundLocalHostRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, SettingNonPersistentDataStorePathsThrowsException)
{
    auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);

    auto shouldThrowExceptionWhenUsed = [](Function<void(void)>&& modifier) {
        @try {
            modifier();
            EXPECT_TRUE(false);
        } @catch (NSException *exception) {
            EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        }
    };

    NSURL *fileURL = [NSURL fileURLWithPath:@"/tmp"];

    shouldThrowExceptionWhenUsed([&] {
        [configuration _setWebStorageDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setIndexedDBDatabaseDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setWebSQLDatabaseDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setCookieStorageFile:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setResourceLoadStatisticsDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setCacheStorageDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setServiceWorkerRegistrationDirectory:fileURL];
    });

    // These properties shouldn't throw exceptions when set on a non-persistent data store.
    [configuration setDeviceManagementRestrictionsEnabled:YES];
    [configuration setHTTPProxy:[NSURL URLWithString:@"http://www.apple.com/"]];
    [configuration setHTTPSProxy:[NSURL URLWithString:@"https://www.apple.com/"]];
    [configuration setSourceApplicationBundleIdentifier:@"com.apple.Safari"];
    [configuration setSourceApplicationSecondaryIdentifier:@"com.apple.Safari"];
}

TEST(WKWebsiteDataStore, FetchPersistentWebStorage)
{
    auto dataTypes = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeSessionStorage, nil];
    auto localStorageType = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, nil];

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] init]);
        auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadHTMLString:@"<script>sessionStorage.setItem('session', 'storage'); localStorage.setItem('local', 'storage');</script>" baseURL:[NSURL URLWithString:@"http://localhost"]];
        [navigationDelegate waitForDidFinishNavigation];
    }

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:dataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], 1u);
        EXPECT_TRUE([[[records firstObject] dataTypes] isEqualToSet:localStorageType]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, FetchNonPersistentWebStorage)
{
    auto nonPersistentDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:nonPersistentDataStore];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadHTMLString:@"<script>sessionStorage.setItem('session', 'storage');localStorage.setItem('local', 'storage');</script>" baseURL:[NSURL URLWithString:@"http://localhost"]];
    [navigationDelegate waitForDidFinishNavigation];

    readyToContinue = false;
    [webView evaluateJavaScript:@"window.sessionStorage.getItem('session')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([@"storage" isEqualToString:result]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [webView evaluateJavaScript:@"window.localStorage.getItem('local')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([@"storage" isEqualToString:result]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [nonPersistentDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeSessionStorage] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ((int)dataRecords.count, 1);
        EXPECT_TRUE([[[dataRecords objectAtIndex:0] displayName] isEqualToString:@"localhost"]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [nonPersistentDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeLocalStorage] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ((int)dataRecords.count, 1);
        EXPECT_TRUE([[[dataRecords objectAtIndex:0] displayName] isEqualToString:@"localhost"]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, SessionSetCount)
{
    auto countSessionSets = [] {
        __block bool done = false;
        __block uint64_t result = 0;
        [[WKWebsiteDataStore defaultDataStore] _countNonDefaultSessionSets:^(uint64_t count) {
            result = count;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        return result;
    };
    @autoreleasepool {
        auto webView0 = adoptNS([WKWebView new]);
        EXPECT_EQ(countSessionSets(), 0u);
        auto configuration = adoptNS([WKWebViewConfiguration new]);
        EXPECT_NULL(configuration.get()._attributedBundleIdentifier);
        configuration.get()._attributedBundleIdentifier = @"test.bundle.id.1";
        auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
        [webView1 loadHTMLString:@"hi" baseURL:nil];
        EXPECT_EQ(countSessionSets(), 1u);
        auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
        [webView2 loadHTMLString:@"hi" baseURL:nil];
        EXPECT_EQ(countSessionSets(), 1u);
        configuration.get()._attributedBundleIdentifier = @"test.bundle.id.2";
        auto webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
        [webView3 loadHTMLString:@"hi" baseURL:nil];
        EXPECT_EQ(countSessionSets(), 2u);
    }
    while (countSessionSets()) { }
}

TEST(WKWebsiteDataStore, ReferenceCycle)
{
    WeakObjCPtr<WKWebsiteDataStore> dataStore;
    WeakObjCPtr<WKHTTPCookieStore> cookieStore;
    @autoreleasepool {
        dataStore = [WKWebsiteDataStore nonPersistentDataStore];
        cookieStore = [dataStore httpCookieStore];
    }
    while (dataStore.get() || cookieStore.get())
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(WKWebsiteDataStore, ClearCustomDataStoreNoWebViews)
{
    HTTPServer server([connectionCount = 0] (Connection connection) mutable {
        ++connectionCount;
        connection.receiveHTTPRequest([connection, connectionCount] (Vector<char>&& request) {
            switch (connectionCount) {
            case 1:
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 5\r\n"
                    "Set-Cookie: a=b\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Hello"_s);
                break;
            case 2:
                EXPECT_FALSE(contains(request.span(), "Cookie: a=b\r\n"_span));
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 5\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Hello"_s);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        });
    });


    NSURL *fileURL = [NSURL fileURLWithPath:@"/tmp/testcookiefile.cookie"];
    auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [configuration _setCookieStorageFile:fileURL];

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get() addToWindow:YES]);

    auto *url = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/index.html", server.port()]];

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    [webView _close];
    webView = nil;

    // Now that the WebView is closed, remove all website data.
    // Then recreate a WebView with the same configuration to confirm the website data was removed.
    static bool done;
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        done = true;
    }];
    Util::run(&done);
    done = false;

    webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get() addToWindow:YES]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
}

TEST(WKWebsiteDataStore, DoNotCreateDefaultDataStore)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration.get() copy];
    EXPECT_FALSE([WKWebsiteDataStore _defaultDataStoreExists]);
}

TEST(WKWebsiteDataStore, DefaultHSTSStorageDirectory)
{
    auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    EXPECT_NOT_NULL(configuration.get().hstsStorageDirectory);
}

static RetainPtr<WKWebsiteDataStore> createWebsiteDataStoreAndPrepare(NSUUID *uuid, NSString *pushPartition)
{
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid]);
    websiteDataStoreConfiguration.get().webPushPartitionString = pushPartition;
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    EXPECT_TRUE([websiteDataStoreConfiguration.get().identifier isEqual:uuid]);
    EXPECT_TRUE([websiteDataStore.get()._identifier isEqual:uuid]);
    EXPECT_TRUE([websiteDataStoreConfiguration.get().webPushPartitionString isEqual:pushPartition]);
    EXPECT_TRUE([websiteDataStore.get()._webPushPartition isEqual:pushPartition]);

    pid_t webprocessIdentifier;
    @autoreleasepool {
        auto handler = adoptNS([[TestMessageHandler alloc] init]);
        [handler addMessage:@"continue" withHandler:^{
            receivedScriptMessage = true;
        }];
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
        [configuration setWebsiteDataStore:websiteDataStore.get()];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSString *htmlString = @"<script> \
            indexedDB.open('testDB').onsuccess = function(event) { \
                window.webkit.messageHandlers.testHandler.postMessage('continue'); \
            } \
        </script>";
        receivedScriptMessage = false;
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        webprocessIdentifier = [webView _webProcessIdentifier];
        EXPECT_NE(webprocessIdentifier, 0);
    }

    // Running web process may hold WebsiteDataStore alive, so make ensure it exits before return.
    while (!kill(webprocessIdentifier, 0))
        TestWebKitAPI::Util::spinRunLoop();

    return websiteDataStore;
}

TEST(WKWebsiteDataStore, DataStoreWithIdentifierAndPushPartition)
{
    __block auto uuid = [NSUUID UUID];
    @autoreleasepool {
        // Make sure WKWebsiteDataStore with identifier does not exist so it can be deleted.
        createWebsiteDataStoreAndPrepare(uuid, @"partition");
    }
}

TEST(WKWebsiteDataStore, RemoveDataStoreWithIdentifier)
{
    NSString *uuidString = @"68753a44-4d6f-1226-9c60-0050e4c00067";
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:uuidString]);
    RetainPtr<NSURL> generalStorageDirectory;
    @autoreleasepool {
        // Make sure WKWebsiteDataStore with identifier does not exist.
        auto websiteDataStore = createWebsiteDataStoreAndPrepare(uuid.get(), @"");
        generalStorageDirectory = websiteDataStore.get()._configuration.generalStorageDirectory;
    }

    EXPECT_NOT_NULL(generalStorageDirectory.get());
    NSFileManager *fileManager = [NSFileManager defaultManager];
    EXPECT_TRUE([fileManager fileExistsAtPath:generalStorageDirectory.get().path]);

    __block bool done = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([fileManager fileExistsAtPath:generalStorageDirectory.get().path]);
}

TEST(WKWebsiteDataStore, RemoveSessionWithIdentifierFromNetworkProcess)
{
    // Launch network process with operation on default data store.
    __block bool done = false;
    RetainPtr defaultDataStore = [WKWebsiteDataStore defaultDataStore];
    [defaultDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    pid_t defaultNetworkProcessIdentifier = [defaultDataStore _networkProcessIdentifier];
    EXPECT_NE(defaultNetworkProcessIdentifier, 0);

    NSString *uuidString = @"68753a44-4d6f-1226-9c60-0050e4c00067";
    RetainPtr uuid = adoptNS([[NSUUID alloc] initWithUUIDString:uuidString]);
    RetainPtr<NSURL> generalStorageDirectory;
    @autoreleasepool {
        RetainPtr websiteDataStore = createWebsiteDataStoreAndPrepare(uuid.get(), @"");
        generalStorageDirectory = websiteDataStore.get()._configuration.generalStorageDirectory;
        EXPECT_EQ(defaultNetworkProcessIdentifier, [websiteDataStore _networkProcessIdentifier]);
    }

    EXPECT_NOT_NULL(generalStorageDirectory.get());
    NSFileManager *fileManager = [NSFileManager defaultManager];
    EXPECT_TRUE([fileManager fileExistsAtPath:generalStorageDirectory.get().path]);

    // Make sure network process is still alive.
    EXPECT_FALSE(kill(defaultNetworkProcessIdentifier, 0));

    done = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([fileManager fileExistsAtPath:generalStorageDirectory.get().path]);
}

TEST(WKWebsiteDataStore, RemoveDataStoreWithIdentifierRemoveCredentials)
{
    // FIXME: we should use persistent credential for test after rdar://100722784 is in build.
    usePersistentCredentialStorage = false;
    done = false;
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    pid_t networkProcessIdentifier;
    @autoreleasepool {
        auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^{
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;

        HTTPServer server(HTTPServer::respondWithChallengeThenOK);
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setWebsiteDataStore:websiteDataStore.get()];
        auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
        [navigationDelegate waitForDidFinishNavigation];

        [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
            EXPECT_EQ((int)dataRecords.count, 1);
            for (WKWebsiteDataRecord *record in dataRecords)
                EXPECT_WK_STREQ([record displayName], @"127.0.0.1");
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
        networkProcessIdentifier = [websiteDataStore.get() _networkProcessIdentifier];
        EXPECT_NE(networkProcessIdentifier, 0);
    }

    // Wait until network process exits so we are sure website data files are not in use during removal.
    while (!kill(networkProcessIdentifier, 0))
        TestWebKitAPI::Util::spinRunLoop();

    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        EXPECT_EQ(credentialCount, 0);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, RemoveDataStoreWithIdentifierErrorWhenInUse)
{
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    auto websiteDataStore = createWebsiteDataStoreAndPrepare(uuid.get(), @"");
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];

    __block bool done = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_TRUE(!!error);
        EXPECT_TRUE([[error description] containsString:@"in use"]);
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, ListIdentifiers)
{
    __block auto uuid = [NSUUID UUID];
    @autoreleasepool {
        // Make sure WKWebsiteDataStore with identifier does not exist so it can be deleted.
        createWebsiteDataStoreAndPrepare(uuid, @"");
    }

    __block bool done = false;
    [WKWebsiteDataStore _fetchAllIdentifiers:^(NSArray<NSUUID *> * identifiers) {
        done = true;
        EXPECT_TRUE([identifiers containsObject:uuid]);
    }];
    TestWebKitAPI::Util::run(&done);

    // Clean up to not leave data on disk.
    done = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStorePrivate, FetchWithSize)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    auto handler = adoptNS([[TestMessageHandler alloc] init]);
    [handler addMessage:@"continue" withHandler:^{
        receivedScriptMessage = true;
    }];
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *htmlString = @"<script> \
        localStorage.setItem('key', 'value'); \
        indexedDB.open('testDB').onsuccess = function(event) { \
            window.webkit.messageHandlers.testHandler.postMessage('continue'); \
        } \
    </script>";
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] _fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] withOptions:_WKWebsiteDataStoreFetchOptionComputeSizes completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], 1u);
        WKWebsiteDataRecord *record = [records firstObject];
        EXPECT_TRUE([[record displayName] isEqualToString:@"webkit.org"]);
        _WKWebsiteDataSize *dataSize = [record _dataSize];
        EXPECT_GT([dataSize totalSize], 0u);
        NSSet *localStorageType = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, nil];
        EXPECT_GT([dataSize sizeOfDataTypes:localStorageType], 0u);
        NSSet *indexedDBType = [NSSet setWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil];
        EXPECT_GT([dataSize sizeOfDataTypes:indexedDBType], 0u);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, DataStoreForNilIdentifier)
{
    bool hasException = false;
    @try {
        auto websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:[[NSUUID alloc] initWithUUIDString:@"1234"]];
        EXPECT_NOT_NULL(websiteDataStore);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        EXPECT_WK_STREQ(@"Identifier is nil", exception.reason);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}

TEST(WKWebsiteDataStore, DataStoreForEmptyIdentifier)
{
    bool hasException = false;
    @try {
        auto data = [NSMutableData dataWithLength:16];
        unsigned char* dataBytes = (unsigned char*) [data mutableBytes];
        auto emptyUUID = [[NSUUID alloc] initWithUUIDBytes:dataBytes];
        auto websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:emptyUUID];
        EXPECT_NOT_NULL(websiteDataStore);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        EXPECT_WK_STREQ(@"Identifier (00000000-0000-0000-0000-000000000000) is invalid for data store", exception.reason);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}

TEST(WKWebsiteDataStoreConfiguration, OriginQuotaRatio)
{
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    [websiteDataStoreConfiguration.get() setVolumeCapacityOverride:[NSNumber numberWithInteger:2 * MB]];
    auto ratioNumber = [NSNumber numberWithDouble:0.5];
    [websiteDataStoreConfiguration.get() setOriginQuotaRatio:ratioNumber];
    EXPECT_TRUE([[websiteDataStoreConfiguration.get() originQuotaRatio] isEqualToNumber:ratioNumber]);
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *htmlString = @"<script> \
        var messageSent = false; \
        function sendMessage(message) { \
            if (messageSent) return; \
            messageSent = true; \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        }; \
        indexedDB.deleteDatabase('testRatio'); \
        var request = indexedDB.open('testRatio'); \
        request.onupgradeneeded = function(event) { \
            db = event.target.result; \
            os = db.createObjectStore('os'); \
            const item = new Array(1024 * 1024).join('x'); \
            os.put(item, 'key').onerror = function(event) { sendMessage(event.target.error.name); }; \
        }; \
        request.onsuccess = function() { sendMessage('Unexpected success'); }; \
        request.onerror = function(event) { sendMessage(event.target.error.name); }; \
    </script>";
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"http://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"QuotaExceededError", [lastScriptMessage body]);
}

TEST(WKWebsiteDataStoreConfiguration, OriginQuotaRatioInvalidValue)
{
    bool hasException = false;
    @try {
        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
        [websiteDataStoreConfiguration.get() setOriginQuotaRatio:[NSNumber numberWithDouble:-1.0]];
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        EXPECT_WK_STREQ(@"OriginQuotaRatio must be in the range [0.0, 1]", exception.reason);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}

TEST(WKWebsiteDataStoreConfiguration, TotalQuotaRatio)
{
    done = false;
    receivedScriptMessage = false;
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    // Clear existing data.
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    NSString *htmlString = @"<script> \
        window.caches.open('test').then((cache) => { \
            return cache.put('https://webkit.org/test', new Response(new ArrayBuffer(20000))); \
        }).then(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('success'); \
        }).catch(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('error'); \
        }); \
    </script>";
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    [websiteDataStoreConfiguration.get() setTotalQuotaRatio:[NSNumber numberWithDouble:0.5]];
    [websiteDataStoreConfiguration.get() setVolumeCapacityOverride:[NSNumber numberWithInteger:100000]];
    auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [websiteDataStore _setResourceLoadStatisticsEnabled:NO];
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://first.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://second.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    auto sortFunction = ^(WKWebsiteDataRecord *record1, WKWebsiteDataRecord *record2) {
        return [record1.displayName compare:record2.displayName];
    };
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeFetchCache] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 2u);
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        EXPECT_WK_STREQ(@"first.com", [[sortedRecords objectAtIndex:0] displayName]);
        EXPECT_WK_STREQ(@"second.com", [[sortedRecords objectAtIndex:1] displayName]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Close network process to ditch its cache for access times, otherwise access time will not be updated until 30 seconds have passed.
    kill([websiteDataStore _networkProcessIdentifier], SIGKILL);

    // Ensure new access time of first.com is later than second.com.
    Util::runFor(1_s);
    // Update recently used origin list.
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://first.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    // Trigger eviction on second.com.
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://third.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeFetchCache] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 2u);
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        EXPECT_WK_STREQ(@"first.com", [[sortedRecords objectAtIndex:0] displayName]);
        EXPECT_WK_STREQ(@"third.com", [[sortedRecords objectAtIndex:1] displayName]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebsiteDataStoreConfiguration, TotalQuotaRatioWithResourceLoadStatisticsEnabled)
{
    done = false;
    receivedScriptMessage = false;
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    // Clear existing data.
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    NSString *htmlString = @"<script> \
        window.caches.open('test').then((cache) => { \
            return cache.put('https://webkit.org/test', new Response(new ArrayBuffer(20000))); \
        }).then(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('success'); \
        }).catch(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('error'); \
        }); \
    </script>";
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    [websiteDataStoreConfiguration.get() setTotalQuotaRatio:[NSNumber numberWithDouble:0.5]];
    [websiteDataStoreConfiguration.get() setVolumeCapacityOverride:[NSNumber numberWithDouble:100000]];
    auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://first.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://second.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    // Simulate user interaction on first.com and third.com.
    done = false;
    [websiteDataStore _logUserInteraction:[NSURL URLWithString:@"https://first.com"] completionHandler:^{
        done = true;
    }];
    Util::run(&done);
    
    done = false;
    [websiteDataStore _logUserInteraction:[NSURL URLWithString:@"https://third.com"] completionHandler:^{
        done = true;
    }];
    Util::run(&done);

    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://third.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    auto sortFunction = ^(WKWebsiteDataRecord *record1, WKWebsiteDataRecord *record2) {
        return [record1.displayName compare:record2.displayName];
    };
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeFetchCache] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 2u);
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        EXPECT_WK_STREQ(@"first.com", [[sortedRecords objectAtIndex:0] displayName]);
        EXPECT_WK_STREQ(@"third.com", [[sortedRecords objectAtIndex:1] displayName]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static NSString *htmlStringForTotalQuotaRatioTest(uint64_t size, bool shouldPersist)
{
    return [NSString stringWithFormat:@"<script> \
        window.caches.open('test').then((cache) => { \
            return cache.put('https://webkit.org/test', new Response(new ArrayBuffer(%llu))); \
        }).then(() => { \
            return %s; \
        }).then((result) => { \
            window.webkit.messageHandlers.testHandler.postMessage(result.toString()); \
        }).catch(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('error'); \
        }); \
    </script>", size, shouldPersist ? "navigator.storage.persist()" : "new String('success')"];
}

TEST(WKWebsiteDataStoreConfiguration, TotalQuotaRatioWithPersistedDomain)
{
    done = false;
    receivedScriptMessage = false;
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    RetainPtr<NSURL> generalStorageDirectory = websiteDataStoreConfiguration.get().generalStorageDirectory;
    // Set total quota to be 50000 bytes.
    [websiteDataStoreConfiguration.get() setTotalQuotaRatio:[NSNumber numberWithDouble:0.5]];
    [websiteDataStoreConfiguration.get() setVolumeCapacityOverride:[NSNumber numberWithDouble:100000]];
    [websiteDataStoreConfiguration.get() setOriginQuotaRatio:[NSNumber numberWithDouble:0.5]];
    // Mark first.com eligible for persistent storage.
    [websiteDataStoreConfiguration.get() setStandaloneApplicationURL:[NSURL URLWithString:@"https://first.com"]];

    auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:htmlStringForTotalQuotaRatioTest(20000, true) baseURL:[NSURL URLWithString:@"https://first.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    // first.com is allowed to be persisted.
    EXPECT_WK_STREQ(@"true", [lastScriptMessage body]);

    [webView loadHTMLString:htmlStringForTotalQuotaRatioTest(20000, true) baseURL:[NSURL URLWithString:@"https://second.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"false", [lastScriptMessage body]);

    [webView loadHTMLString:htmlStringForTotalQuotaRatioTest(40000, true) baseURL:[NSURL URLWithString:@"https://third.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"false", [lastScriptMessage body]);

    __block bool isEvicted = false;
    // Eviction happens asynchronously in network process so keep checking until it is done.
    while (!isEvicted) {
        [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeFetchCache] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
            if (records.count == 2u) {
                auto sortFunction = ^(WKWebsiteDataRecord *record1, WKWebsiteDataRecord *record2) {
                    return [record1.displayName compare:record2.displayName];
                };
                auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
                EXPECT_WK_STREQ(@"first.com", [[sortedRecords objectAtIndex:0] displayName]);
                EXPECT_WK_STREQ(@"third.com", [[sortedRecords objectAtIndex:1] displayName]);
                isEvicted = true;
            }
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
    }

    // Persisted flag of first.com is cleared when its data is deleted.
    [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView loadHTMLString:htmlStringForTotalQuotaRatioTest(20000, false) baseURL:[NSURL URLWithString:@"https://first.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    [webView loadHTMLString:htmlStringForTotalQuotaRatioTest(45000, false) baseURL:[NSURL URLWithString:@"https://third.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    while (!isEvicted) {
        [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeFetchCache] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
            if (records.count == 1u) {
                EXPECT_WK_STREQ(@"third.com", [[records objectAtIndex:0] displayName]);
                isEvicted = true;
            }
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
    }
}

TEST(WKWebsiteDataStoreConfiguration, TotalQuotaRatioInvalidValue)
{
    bool hasException = false;
    @try {
        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
        [websiteDataStoreConfiguration.get() setTotalQuotaRatio:[NSNumber numberWithDouble:2.0]];
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        EXPECT_WK_STREQ(@"TotalQuotaRatio must be in the range [0.0, 1]", exception.reason);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}

TEST(WKWebsiteDataStoreConfiguration, QuotaRatioDefaultValue)
{
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    EXPECT_TRUE([websiteDataStoreConfiguration.get() originQuotaRatio]);
    EXPECT_EQ([[websiteDataStoreConfiguration.get() originQuotaRatio] doubleValue], 0.6);

    EXPECT_TRUE([websiteDataStoreConfiguration.get() totalQuotaRatio]);
    EXPECT_EQ([[websiteDataStoreConfiguration.get() totalQuotaRatio] doubleValue], 0.8);
}

TEST(WKWebsiteDataStoreConfiguration, StandardVolumeCapacity)
{
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    readyToContinue = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    // Origin quota is 7 MB; standard reported origin quota is 1 MB.
    [websiteDataStoreConfiguration.get() setVolumeCapacityOverride:[NSNumber numberWithInteger:14 * MB]];
    [websiteDataStoreConfiguration.get() setStandardVolumeCapacity:[NSNumber numberWithInteger:2 * MB]];
    [websiteDataStoreConfiguration.get() setOriginQuotaRatio:[NSNumber numberWithDouble:0.5]];
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *htmlString = @"<script> \
        var db = null; \
        var number = 0; \
        function checkQuota() { \
            navigator.storage.estimate().then((estimate) => { \
                window.webkit.messageHandlers.testHandler.postMessage(estimate.quota.toString()); \
            }); \
        } \
        function storeMB(n) { \
        try { \
            const size = Math.ceil(n * 1024 * 1024); \
            const item = new Array(size).join('x'); \
            const os = db.transaction('os', 'readwrite').objectStore('os'); \
            var putRequest = os.put(item, ++number); \
            putRequest.onsuccess = checkQuota; \
            putRequest.onerror = function(event) { window.webkit.messageHandlers.testHandler.postMessage('Error'); }; \
        } catch(e) { \
            window.webkit.messageHandlers.testHandler.postMessage(e.toString()); \
        } \
        } \
        var request = indexedDB.open('testReportedQuota'); \
        request.onupgradeneeded = function(event) { \
            db = event.target.result; \
            db.createObjectStore('os'); \
        }; \
        request.onsuccess = function() { window.webkit.messageHandlers.testHandler.postMessage('Opened'); }; \
        request.onerror = function(event) { window.webkit.messageHandlers.testHandler.postMessage(event.target.error.name); }; \
    </script>";
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Opened", [lastScriptMessage body]);

    // Usage is 0.
    receivedScriptMessage = false;
    [webView evaluateJavaScript:@"checkQuota()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ([[NSNumber numberWithInteger:1 * MB] stringValue], [lastScriptMessage body]);

    // Increase usage to a little over 0.8 MB - reported quota is doubled.
    receivedScriptMessage = false;
    [webView evaluateJavaScript:@"storeMB(0.8)" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ([[NSNumber numberWithInteger:2 * MB] stringValue], [lastScriptMessage body]);

    // Increase usage to over 1.8 MB - reported quota is doubled.
    receivedScriptMessage = false;
    [webView evaluateJavaScript:@"storeMB(1)" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ([[NSNumber numberWithInteger:4 * MB] stringValue], [lastScriptMessage body]);

    // Increase usage to over 2.8 MB - reported quota is actual quota.
    receivedScriptMessage = false;
    [webView evaluateJavaScript:@"storeMB(1)" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ([[NSNumber numberWithInteger:7 * MB] stringValue], [lastScriptMessage body]);

    // Increase usage to over 3.8 MB - reported quota is actual quota.
    receivedScriptMessage = false;
    [webView evaluateJavaScript:@"storeMB(1)" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ([[NSNumber numberWithInteger:7 * MB] stringValue], [lastScriptMessage body]);
}

TEST(WKWebsiteDataStorePrivate, CompletionHandlerForRemovalFromNetworkProcess)
{
    __block bool done = false;
    __block unsigned completionHandlerNumber = 0;

    // Create a web view that keeps default network process running.
    auto defaultConfiguration = adoptNS([WKWebViewConfiguration new]);
    auto defaultNavigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto defaultWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:defaultConfiguration.get()]);
    [defaultWebView setNavigationDelegate:defaultNavigationDelegate.get()];
    [defaultWebView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://apple.com"]];
    [defaultNavigationDelegate waitForDidFinishNavigation];

    @autoreleasepool {
        // Create a new data store to be removed from network process.
        auto websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
        auto configuration = adoptNS([WKWebViewConfiguration new]);
        [configuration setWebsiteDataStore:websiteDataStore];
        auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
        [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://webkit.org/"]];
        [navigationDelegate waitForDidFinishNavigation];
        EXPECT_EQ(defaultConfiguration.get().websiteDataStore._networkProcessIdentifier, configuration.get().websiteDataStore._networkProcessIdentifier);

        done = false;
        [websiteDataStore _setCompletionHandlerForRemovalFromNetworkProcess:^(NSError *error) {
            EXPECT_NOT_NULL(error);
            EXPECT_TRUE([[error description] containsString:@"New completion handler is set"]);
            completionHandlerNumber = 1;
            done = true;
        }];

        [websiteDataStore _setCompletionHandlerForRemovalFromNetworkProcess:^(NSError *error) {
            EXPECT_NULL(error);
            completionHandlerNumber = 2;
            done = true;
        }];
        Util::run(&done);
        EXPECT_EQ(completionHandlerNumber, 1u);
        
        // Wait for WebsiteDataStore to be destroyed and removed from network process.
        done = false;
    }

    Util::run(&done);
    EXPECT_EQ(completionHandlerNumber, 2u);
}

#if PLATFORM(MAC)

TEST(WKWebsiteDataStore, DoNotLogNetworkConnectionsInEphemeralSessions)
{
    HTTPServer server { { }, HTTPServer::Protocol::Http };
    server.addResponse("/index.html"_s, { "<html><body>Hello world</body></html>"_s });

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:WKWebsiteDataStore.nonPersistentDataStore];

    auto urlToLoad = [NSURL URLWithString:[NSString stringWithFormat:@"http://localhost:%u/index.html", server.port()]];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:urlToLoad]];

    EXPECT_EQ(server.totalConnections(), 1U);
    EXPECT_EQ([webView collectLogsForNewConnections].count, 0U);
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
