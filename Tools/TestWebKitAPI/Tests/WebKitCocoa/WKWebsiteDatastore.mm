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

namespace TestWebKitAPI {


// FIXME: Re-enable this test once webkit.org/b/208451 is resolved.
#if !PLATFORM(IOS)
TEST(WKWebsiteDataStore, RemoveAndFetchData)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0u, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}
#endif // !PLATFORM(IOS)

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
        __block size_t result = 0;
        [[WKWebsiteDataStore defaultDataStore] _countNonDefaultSessionSets:^(size_t count) {
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

TEST(WebKit, ClearCustomDataStoreNoWebViews)
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
                EXPECT_FALSE(strstr(request.data(), "Cookie: a=b\r\n"));
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

TEST(WebKit, DefaultHSTSStorageDirectory)
{
    auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    EXPECT_NOT_NULL(configuration.get().hstsStorageDirectory);
}

} // namespace TestWebKitAPI
