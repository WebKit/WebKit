/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "LoadMetadataProtocol.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/LoadMetadata.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>

@interface LoadMetadataRemoteObject : NSObject <LoadMetadataProtocol>
@property (nonatomic, readonly) HashMap<pid_t, LoadMetadataType> pidLoadMetadatas;
@property (nonatomic, readonly) uint32_t numLoadMetadataCallbacks;
@end

@implementation LoadMetadataRemoteObject

- (bool)hasMetadataForPid:(pid_t)pid
{
    return _pidLoadMetadatas.contains(pid);
}

- (OptionSet<WebCore::LoadMetadata>)metadataForPid:(pid_t)pid
{
    RELEASE_ASSERT([self hasMetadataForPid:pid]);

    return OptionSet<WebCore::LoadMetadata>::fromRaw(_pidLoadMetadatas.get(pid));
}

- (void)loadMetadata:(LoadMetadataType)value withPID:(uint64_t)pid
{
    _pidLoadMetadatas.set(pid, value);
    _numLoadMetadataCallbacks++;

    NSLog(@"LoadMedata test received LoadMetadata: %d with running PID: %lld", value, pid);
}

@end

static RetainPtr<TestWKWebView> loadMetadataTestConfiguration(const TestWebKitAPI::HTTPServer* plaintextServer, const TestWebKitAPI::HTTPServer* secureServer = nullptr, bool useSiteIsolation = false)
{
    NSString * const testPlugInClassName = @"LoadMetadataPlugin";

    RetainPtr<WKWebViewConfiguration> configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:testPlugInClassName]);

    [configuration preferences].javaScriptCanOpenWindowsAutomatically = YES;

    if (useSiteIsolation) {
        auto preferences = [configuration preferences];
        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"SiteIsolationEnabled"]) {
                [preferences _setEnabled:YES forFeature:feature];
                break;
            }
        }
    }

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);

    if (plaintextServer)
        [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", plaintextServer->port()]]];

    if (secureServer)
        [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", secureServer->port()]]];

    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get()]);

    if (secureServer) {
        auto navigationDelegate = [TestNavigationDelegate new];
        [navigationDelegate allowAnyTLSCertificate];
        [webView setNavigationDelegate: navigationDelegate];
    }

    return webView;
}

enum class ExpectedMetadata : bool { Secure, Insecure };
static void runAndCheckLoadMetadata(RetainPtr<WKWebView> webView, NSString *url, ExpectedMetadata expectedMetadata)
{
    RetainPtr<LoadMetadataRemoteObject> object = adoptNS([[LoadMetadataRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(LoadMetadataProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "success!");

    TestWebKitAPI::Util::waitFor([&] {
        return [object numLoadMetadataCallbacks] == 1;
    });

    pid_t webViewPid = [webView _webProcessIdentifier];
    OptionSet<WebCore::LoadMetadata> metadata = [object metadataForPid:webViewPid];

    if (expectedMetadata == ExpectedMetadata::Insecure)
        EXPECT_TRUE(metadata.contains(WebCore::LoadMetadata::Insecure));
    else
        EXPECT_FALSE(metadata.contains(WebCore::LoadMetadata::Insecure));
}

static void runAndCheckLoadMetadataForFrames(RetainPtr<TestWKWebView> webView, NSString *url)
{
    RetainPtr<LoadMetadataRemoteObject> object = adoptNS([[LoadMetadataRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(LoadMetadataProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "success!");

    TestWebKitAPI::Util::waitFor([&] {
        return [object numLoadMetadataCallbacks] == 2;
    });

    auto mainFrame = [webView mainFrame];
    EXPECT_EQ(1u, mainFrame.childFrames.count);

    auto childFrame = mainFrame.childFrames.firstObject;

    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;

    TestWebKitAPI::Util::waitFor([&] {
        return [object hasMetadataForPid:mainFramePid] && [object hasMetadataForPid:childFramePid];
    });

    EXPECT_TRUE([object metadataForPid:mainFramePid].contains(WebCore::LoadMetadata::Insecure));
    EXPECT_TRUE([object metadataForPid:childFramePid].contains(WebCore::LoadMetadata::Insecure));
}

enum class ExpectCreated : bool { No, Yes };
static void runAndCheckLoadMetadataForViews(RetainPtr<TestWKWebView> webView, NSString *url, ExpectCreated expectCreated = ExpectCreated::Yes, uint8_t expectMetadataCallbackCount = 2)
{
    RetainPtr<LoadMetadataRemoteObject> object = adoptNS([[LoadMetadataRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(LoadMetadataProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    RELEASE_ASSERT(!webView.get().UIDelegate);

    __block auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    __block auto navigationDelegate = [webView navigationDelegate];

    __block pid_t mainWebViewPid = 0;
    __block RetainPtr<WKWebView> createdWebView;

    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        createdWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

        mainWebViewPid = [webView _webProcessIdentifier];

        createdWebView.get().UIDelegate = uiDelegate.get();
        createdWebView.get().navigationDelegate = navigationDelegate;

        [[createdWebView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

        return createdWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "success!");

    if (expectCreated == ExpectCreated::Yes) {
        EXPECT_FALSE(!createdWebView);
        EXPECT_NE(mainWebViewPid, 0);

        pid_t createdWebViewPid = [createdWebView _webProcessIdentifier];
        EXPECT_NE(createdWebViewPid, 0);

        TestWebKitAPI::Util::waitFor([&] {
            return [object numLoadMetadataCallbacks] == expectMetadataCallbackCount;
        });

        EXPECT_TRUE([object metadataForPid:mainWebViewPid].contains(WebCore::LoadMetadata::Insecure));
        EXPECT_TRUE([object metadataForPid:createdWebViewPid].contains(WebCore::LoadMetadata::Insecure));

    } else {
        EXPECT_TRUE(!createdWebView);
        EXPECT_EQ(mainWebViewPid, 0);

        mainWebViewPid = [webView _webProcessIdentifier];
        EXPECT_NE(mainWebViewPid, 0);

        TestWebKitAPI::Util::waitFor([&] {
            return [object numLoadMetadataCallbacks] == expectMetadataCallbackCount;
        });

        EXPECT_TRUE([object metadataForPid:mainWebViewPid].contains(WebCore::LoadMetadata::Insecure));
    }
}

#define TEST_WITH_AND_WITHOUT_SITE_ISOLATION(test_name) \
TEST(LoadMetadata, test_name) \
{ \
    run##test_name(false); \
} \
\
TEST(LoadMetadata, test_name##WithSiteIsolation) \
{ \
    run##test_name(true); \
}

static void runInsecureLoad(bool useSiteIsolation)
{
    TestWebKitAPI::HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>alert('success!')</script>"_s } },
    });

    auto webView = loadMetadataTestConfiguration(&plaintextServer, nullptr, useSiteIsolation);

    runAndCheckLoadMetadata(webView, @"http://insecure.example.internal/", ExpectedMetadata::Insecure);
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureLoad)

static void runSecureLoad(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(nullptr, &secureServer, useSiteIsolation);

    runAndCheckLoadMetadata(webView, @"https://secure.example.internal/", ExpectedMetadata::Secure);
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(SecureLoad)

static void runSameSiteHTTPSUpgrade(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.co.uk/"_s, { 302, { { "Location"_s, "https://secure.example.co.uk/"_s } }, emptyString() } } });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    runAndCheckLoadMetadata(webView, @"http://insecure.example.co.uk/", ExpectedMetadata::Secure);
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(SameSiteHTTPSUpgrade)

static void runInsecureEmbeddingInsecure(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<iframe src='http://insecure.different.internal/'></iframe>"_s } },
        { "http://insecure.different.internal/"_s, { "<script>alert('success!')</script>"_s } }
    });

    auto webView = loadMetadataTestConfiguration(&plaintextServer, nullptr, useSiteIsolation);

    runAndCheckLoadMetadataForFrames(webView, @"http://insecure.example.internal/");
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureEmbeddingInsecure)

static void runInsecureEmbeddingSecure(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<iframe src='https://secure.different.internal/'></iframe>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    runAndCheckLoadMetadataForFrames(webView, @"http://insecure.example.internal/");
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureEmbeddingSecure)

static void runInsecureOpeningSecure(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.open('https://secure.different.internal/'); }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    runAndCheckLoadMetadataForViews(webView, @"http://insecure.example.internal/");
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureOpeningSecure)

static void runInsecureOpeningSecureTargetSelf(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.open('https://secure.different.internal/', '_self', 'noopener'); }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    runAndCheckLoadMetadataForViews(webView, @"http://insecure.example.internal/", ExpectCreated::No);
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureOpeningSecureTargetSelf)

static void runInsecureOpeningSecureNoOpener(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.open('https://secure.different.internal/', '_blank', 'noopener'); }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    runAndCheckLoadMetadataForViews(webView, @"http://insecure.example.internal/");
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureOpeningSecureNoOpener)

static void runInsecureLocationRedirectsSecure(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<script>window.onload = function() { window.location = 'https://secure.different.internal/'; }</script>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    runAndCheckLoadMetadataForViews(webView, @"http://insecure.example.internal/", ExpectCreated::No);
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureLocationRedirectsSecure)

static void runInsecureThenClickInitiatedNavigation(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "<a id='testLink' href='https://secure.different.internal/'>Link</a>"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    RetainPtr<LoadMetadataRemoteObject> object = adoptNS([[LoadMetadataRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(LoadMetadataProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    __block auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    __block RetainPtr<WKWebView> createdWebView;

    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        createdWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

        createdWebView.get().UIDelegate = uiDelegate.get();
        createdWebView.get().navigationDelegate = navigationDelegate.get();

        [[createdWebView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

        return createdWebView.get();
    };

    NSString *url = @"http://insecure.example.internal/";

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];

    Util::waitFor([&] {
        return [object numLoadMetadataCallbacks] == 1;
    });

    pid_t initialProcessPid = [webView _webProcessIdentifier];

    EXPECT_TRUE([object metadataForPid:initialProcessPid].contains(WebCore::LoadMetadata::Insecure));

    // Running Javascript simulates a user gesture so the navigation should escape the LoadMetadata in the new process

    [webView clickOnElementID:@"testLink"];

    EXPECT_WK_STREQ([uiDelegate waitForAlert], "success!");

    Util::waitFor([&] {
        return [object numLoadMetadataCallbacks] == 2;
    });

    pid_t createdProcessPid = [webView _webProcessIdentifier];
    EXPECT_TRUE(initialProcessPid != createdProcessPid);

    EXPECT_TRUE([object hasMetadataForPid:createdProcessPid]);
    EXPECT_FALSE([object metadataForPid:createdProcessPid].contains(WebCore::LoadMetadata::Insecure));
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureThenClickInitiatedNavigation)

static void runInsecureThenNavigatedToSecure(bool useSiteIsolation)
{
    using namespace TestWebKitAPI;

    HTTPServer plaintextServer({
        { "http://insecure.example.internal/"_s, { "Hello"_s } }
    });

    HTTPServer secureServer({
        { "/"_s, { "<script>alert('success!')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webView = loadMetadataTestConfiguration(&plaintextServer, &secureServer, useSiteIsolation);

    RetainPtr<LoadMetadataRemoteObject> object = adoptNS([[LoadMetadataRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(LoadMetadataProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    __block auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    __block RetainPtr<WKWebView> createdWebView;

    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        createdWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

        createdWebView.get().UIDelegate = uiDelegate.get();
        createdWebView.get().navigationDelegate = navigationDelegate.get();

        [[createdWebView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

        return createdWebView.get();
    };

    NSString *url = @"http://insecure.example.internal/";

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];

    Util::waitFor([&] {
        return [object numLoadMetadataCallbacks] == 1;
    });

    pid_t initialProcessPid = [webView _webProcessIdentifier];

    EXPECT_TRUE([object metadataForPid:initialProcessPid].contains(WebCore::LoadMetadata::Insecure));

    // Performing an app initiated new request should break out of any previous LoadMetadata.

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://secure.different.internal/"]]];

    EXPECT_WK_STREQ([uiDelegate waitForAlert], "success!");

    Util::waitFor([&] {
        return [object numLoadMetadataCallbacks] == 2;
    });

    pid_t createdProcessPid = [webView _webProcessIdentifier];
    EXPECT_TRUE(initialProcessPid != createdProcessPid);

    EXPECT_TRUE([object hasMetadataForPid:createdProcessPid]);
    EXPECT_FALSE([object metadataForPid:createdProcessPid].contains(WebCore::LoadMetadata::Insecure));
}

TEST_WITH_AND_WITHOUT_SITE_ISOLATION(InsecureThenNavigatedToSecure)
