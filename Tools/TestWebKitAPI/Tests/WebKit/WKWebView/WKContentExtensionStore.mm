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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import <WebKit/WKContentRuleList.h>
#import <WebKit/WKContentRuleListStorePrivate.h>
#import <WebKit/WKFoundation.h>
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKContentRuleListAction.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

class WKContentRuleListStoreTest : public testing::Test {
public:
    virtual void SetUp()
    {
        [[WKContentRuleListStore defaultStore] _removeAllContentRuleLists];
    }
};

static NSString *basicFilter = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*webkit.org\"}}]";

TEST_F(WKContentRuleListStoreTest, Compile)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
}

static NSString *invalidFilter = @"[";

static void checkDomain(NSError *error)
{
    EXPECT_STREQ([[error domain] UTF8String], [WKErrorDomain UTF8String]);
}

TEST_F(WKContentRuleListStoreTest, InvalidRuleList)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:invalidFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
    
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentRuleListStoreCompileFailed);
        EXPECT_STREQ("Rule list compilation failed: Failed to parse the JSON String.", [[error helpAnchor] UTF8String]);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
}

TEST_F(WKContentRuleListStoreTest, Lookup)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    __block bool doneLookingUp = false;
    [[WKContentRuleListStore defaultStore] lookUpContentRuleListForIdentifier:@"TestRuleList" completionHandler:^(WKContentRuleList *filter, NSError *error) {

        EXPECT_STREQ(filter.identifier.UTF8String, "TestRuleList");
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(WKContentRuleListStoreTest, EncodedIdentifier)
{
    // FIXME: U+00C4 causes problems here. Using the output of encodeForFileName with
    // the filesystem changes it to U+0041 followed by U+0308
    NSString *identifier = @":;%25%+25И😍";
    __block bool done = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:identifier encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {

        EXPECT_STREQ(filter.identifier.UTF8String, identifier.UTF8String);

        [[WKContentRuleListStore defaultStore] getAvailableContentRuleListIdentifiers:^(NSArray<NSString *> *identifiers) {
            EXPECT_EQ(identifiers.count, 1u);
            EXPECT_EQ(identifiers[0].length, identifier.length);
            EXPECT_STREQ(identifiers[0].UTF8String, identifier.UTF8String);

            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST_F(WKContentRuleListStoreTest, NonExistingIdentifierLookup)
{
    __block bool doneLookingUp = false;
    [[WKContentRuleListStore defaultStore] lookUpContentRuleListForIdentifier:@"DoesNotExist" completionHandler:^(WKContentRuleList *filter, NSError *error) {
    
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentRuleListStoreLookUpFailed);
        EXPECT_STREQ("Rule list lookup failed: Unspecified error during lookup.", [[error helpAnchor] UTF8String]);
        
        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(WKContentRuleListStoreTest, VersionMismatch)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];

    TestWebKitAPI::Util::run(&doneCompiling);

    [[WKContentRuleListStore defaultStore] _invalidateContentRuleListVersionForIdentifier:@"TestRuleList"];

    __block bool doneLookingUp = false;
    [[WKContentRuleListStore defaultStore] lookUpContentRuleListForIdentifier:@"TestRuleList" completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneLookingUp = true;
    }];

    TestWebKitAPI::Util::run(&doneLookingUp);

    __block bool doneGettingSource = false;
    [[WKContentRuleListStore defaultStore] _getContentRuleListSourceForIdentifier:@"TestRuleList" completionHandler:^(NSString* source) {
        EXPECT_NOT_NULL(source);
        EXPECT_NS_EQUAL(source, basicFilter);

        doneGettingSource = true;
    }];

    TestWebKitAPI::Util::run(&doneGettingSource);
}

TEST_F(WKContentRuleListStoreTest, CorruptHeaderEmpty)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];

    TestWebKitAPI::Util::run(&doneCompiling);

    [[WKContentRuleListStore defaultStore] _corruptContentRuleListHeaderForIdentifier:@"TestRuleList" usingCurrentVersion:NO];

    __block bool doneLookingUp = false;
    [[WKContentRuleListStore defaultStore] lookUpContentRuleListForIdentifier:@"TestRuleList" completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentRuleListStoreVersionMismatch);
        EXPECT_NS_EQUAL(error.helpAnchor, @"Rule list lookup failed: Version of file does not match version of interpreter.");

        doneLookingUp = true;
    }];

    TestWebKitAPI::Util::run(&doneLookingUp);

    __block bool doneGettingSource = false;
    [[WKContentRuleListStore defaultStore] _getContentRuleListSourceForIdentifier:@"TestRuleList" completionHandler:^(NSString* source) {
        EXPECT_NULL(source);

        doneGettingSource = true;
    }];

    TestWebKitAPI::Util::run(&doneGettingSource);
}

TEST_F(WKContentRuleListStoreTest, CorruptHeaderRandom)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error)
    {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];

    TestWebKitAPI::Util::run(&doneCompiling);

    [[WKContentRuleListStore defaultStore] _corruptContentRuleListHeaderForIdentifier:@"TestRuleList" usingCurrentVersion:YES];

    __block bool doneLookingUp = false;
    [[WKContentRuleListStore defaultStore] lookUpContentRuleListForIdentifier:@"TestRuleList" completionHandler:^(WKContentRuleList *filter, NSError *error)
    {
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentRuleListStoreLookUpFailed);
        EXPECT_NS_EQUAL(error.helpAnchor, @"Rule list lookup failed: Unspecified error during lookup.");

        doneLookingUp = true;
    }];

    TestWebKitAPI::Util::run(&doneLookingUp);

    __block bool doneGettingSource = false;
    [[WKContentRuleListStore defaultStore] _getContentRuleListSourceForIdentifier:@"TestRuleList" completionHandler:^(NSString* source) {
        EXPECT_NULL(source);

        doneGettingSource = true;
    }];

    TestWebKitAPI::Util::run(&doneGettingSource);
}

TEST_F(WKContentRuleListStoreTest, CorruptURLFilter)
{
    NSString* contentBlocker = @"[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]";
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"CorruptURLFilter" encodedContentRuleList:contentBlocker completionHandler:^(WKContentRuleList *filter, NSError *error) {

        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
    [[WKContentRuleListStore defaultStore] _corruptContentRuleListActionsMatchingEverythingForIdentifier:@"CorruptURLFilter"];

    __block bool doneLookingUp = false;
    [[WKContentRuleListStore defaultStore] lookUpContentRuleListForIdentifier:@"CorruptURLFilter" completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentRuleListStoreLookUpFailed);
        EXPECT_NS_EQUAL(error.helpAnchor, @"Rule list lookup failed: Unspecified error during lookup.");

        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(WKContentRuleListStoreTest, InvalidHeader)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];

    TestWebKitAPI::Util::run(&doneCompiling);

    [[WKContentRuleListStore defaultStore] _invalidateContentRuleListHeaderForIdentifier:@"TestRuleList"];

    __block bool doneLookingUp = false;
    [[WKContentRuleListStore defaultStore] lookUpContentRuleListForIdentifier:@"TestRuleList" completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentRuleListStoreVersionMismatch);
        EXPECT_NS_EQUAL(error.helpAnchor, @"Rule list lookup failed: Version of file does not match version of interpreter.");

        doneLookingUp = true;
    }];

    TestWebKitAPI::Util::run(&doneLookingUp);

    __block bool doneGettingSource = false;
    [[WKContentRuleListStore defaultStore] _getContentRuleListSourceForIdentifier:@"TestRuleList" completionHandler:^(NSString* source) {
        EXPECT_NULL(source);

        doneGettingSource = true;
    }];

    TestWebKitAPI::Util::run(&doneGettingSource);
}

TEST_F(WKContentRuleListStoreTest, Removal)
{
    __block bool doneCompiling = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    __block bool doneRemoving = false;
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"TestRuleList" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);

        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}

#if CPU(ARM64) && defined(NDEBUG)
// FIXME: The code below crashes on arm64e release builds.
// See rdar://95391568 and rdar://95247117
TEST_F(WKContentRuleListStoreTest, DISABLED_CrossOriginCookieBlocking)
#else
TEST_F(WKContentRuleListStoreTest, CrossOriginCookieBlocking)
#endif
{
    using namespace TestWebKitAPI;

    auto cookiePresentWhenBlocking = [] (bool blockCookies) {

        std::optional<bool> requestHadCookieResult;

        HTTPServer server(HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> ConnectionTask {
            while (true) {
                auto request = co_await connection.awaitableReceiveHTTPRequest();
                auto path = HTTPServer::parsePath(request);
                std::optional<bool> pendingCookieResult;
                auto response = [&] {
                    if (path == "/com"_s)
                        return HTTPResponse({ { "Set-Cookie"_s, "testCookie=42; Path=/; SameSite=None; Secure"_s } }, "<script>alert('hi')</script>"_s);
                    if (path == "/org"_s)
                        return HTTPResponse("<script>fetch('https://example.com/cookie-check', {credentials: 'include'})</script>"_s);
                    if (path == "/cookie-check"_s) {
                        pendingCookieResult = contains(request.span(), "Cookie: testCookie=42"_span);
                        return HTTPResponse("hi"_s);
                    }
                    RELEASE_ASSERT_NOT_REACHED();
                }();
                co_await connection.awaitableSend(response.serialize());
                if (pendingCookieResult)
                    requestHadCookieResult = pendingCookieResult;
            }
        }, HTTPServer::Protocol::HttpsProxy);

        RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [storeConfiguration setAllowsServerPreconnect:NO];
        [storeConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
        }];

        RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
        [dataStore _setResourceLoadStatisticsEnabled:NO];
        __block bool setPolicy { false };
        [dataStore.get().httpCookieStore _setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways completionHandler:^{
            setPolicy = true;
        }];
        Util::run(&setPolicy);

        RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
        [viewConfiguration setWebsiteDataStore:dataStore.get()];

        if (blockCookies) {
            __block bool doneCompiling { false };
            NSString *json = @"[{\"action\":{\"type\":\"block-cookies\"},\"trigger\":{\"url-filter\":\"cookie-check\"}}]";
            [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestBlockCookies" encodedContentRuleList:json completionHandler:^(WKContentRuleList *compiledRuleList, NSError *error) {
                EXPECT_FALSE(error);
                [[viewConfiguration userContentController] addContentRuleList:compiledRuleList];
                doneCompiling = true;
            }];
            TestWebKitAPI::Util::run(&doneCompiling);
        }

        RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
        RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
        delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        };
        webView.get().navigationDelegate = delegate.get();

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/com"]]];
        [delegate waitForDidFinishNavigation];

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.org/org"]]];
        while (!requestHadCookieResult)
            Util::spinRunLoop();
        return *requestHadCookieResult;
    };

    EXPECT_FALSE(cookiePresentWhenBlocking(true));
    EXPECT_TRUE(cookiePresentWhenBlocking(false));
}

TEST_F(WKContentRuleListStoreTest, NonExistingIdentifierRemove)
{
    __block bool doneRemoving = false;
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"DoesNotExist" completionHandler:^(NSError *error) {
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentRuleListStoreRemoveFailed);
        EXPECT_STREQ("Rule list removal failed: Unspecified error during remove.", [[error helpAnchor] UTF8String]);

        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}

TEST_F(WKContentRuleListStoreTest, NonDefaultStore)
{
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"ContentRuleListTest"] isDirectory:YES];
    WKContentRuleListStore *store = [WKContentRuleListStore storeWithURL:tempDir];
    NSString *identifier = @"TestRuleList";
    NSString *fileName = @"ContentRuleList-TestRuleList";

    __block bool doneGettingAvailableIdentifiers = false;
    [store getAvailableContentRuleListIdentifiers:^(NSArray<NSString *> *identifiers) {
        EXPECT_NOT_NULL(identifiers);
        EXPECT_EQ(identifiers.count, 0u);
        doneGettingAvailableIdentifiers = true;
    }];
    TestWebKitAPI::Util::run(&doneGettingAvailableIdentifiers);
    
    __block bool doneCompiling = false;
    [store compileContentRuleListForIdentifier:identifier encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    doneGettingAvailableIdentifiers = false;
    [store getAvailableContentRuleListIdentifiers:^(NSArray<NSString *> *identifiers) {
        EXPECT_NOT_NULL(identifiers);
        EXPECT_EQ(identifiers.count, 1u);
        EXPECT_STREQ(identifiers[0].UTF8String, "TestRuleList");
        doneGettingAvailableIdentifiers = true;
    }];
    TestWebKitAPI::Util::run(&doneGettingAvailableIdentifiers);

    NSData *data = [NSData dataWithContentsOfURL:[tempDir URLByAppendingPathComponent:fileName]];
    EXPECT_NOT_NULL(data);
    EXPECT_EQ(data.length, 241u);
    
    __block bool doneCheckingSource = false;
    [store _getContentRuleListSourceForIdentifier:identifier completionHandler:^(NSString *source) {
        EXPECT_NOT_NULL(source);
        EXPECT_STREQ(basicFilter.UTF8String, source.UTF8String);
        doneCheckingSource = true;
    }];
    TestWebKitAPI::Util::run(&doneCheckingSource);
    
    __block bool doneRemoving = false;
    [store removeContentRuleListForIdentifier:identifier completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);

    NSData *dataAfterRemoving = [NSData dataWithContentsOfURL:[tempDir URLByAppendingPathComponent:fileName]];
    EXPECT_NULL(dataAfterRemoving);
}

TEST_F(WKContentRuleListStoreTest, MultipleRuleLists)
{
    __block bool done = false;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"FirstRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);
        [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"SecondRuleList" encodedContentRuleList:basicFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
            EXPECT_NOT_NULL(filter);
            EXPECT_NULL(error);
            [[WKContentRuleListStore defaultStore] getAvailableContentRuleListIdentifiers:^(NSArray<NSString *> *identifiers) {
                EXPECT_NOT_NULL(identifiers);
                EXPECT_EQ(identifiers.count, 2u);
                EXPECT_STREQ(identifiers[0].UTF8String, "FirstRuleList");
                EXPECT_STREQ(identifiers[1].UTF8String, "SecondRuleList");
                done = true;
            }];
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST_F(WKContentRuleListStoreTest, NonASCIISource)
{
    static NSString *nonASCIIFilter = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*webkit.org\"}, \"unused\":\"ðŸ’©\"}]";
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"ContentRuleListTest"] isDirectory:YES];
    WKContentRuleListStore *store = [WKContentRuleListStore storeWithURL:tempDir];
    NSString *identifier = @"TestRuleList";
    NSString *fileName = @"ContentRuleList-TestRuleList";
    
    __block bool done = false;
    [store compileContentRuleListForIdentifier:identifier encodedContentRuleList:nonASCIIFilter completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        [store _getContentRuleListSourceForIdentifier:identifier completionHandler:^(NSString *source) {
            EXPECT_NOT_NULL(source);
            EXPECT_STREQ(nonASCIIFilter.UTF8String, source.UTF8String);

            [store _removeAllContentRuleLists];
            NSData *dataAfterRemoving = [NSData dataWithContentsOfURL:[tempDir URLByAppendingPathComponent:fileName]];
            EXPECT_NULL(dataAfterRemoving);

            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

static size_t contentExtensionAlertCount { 0 };
static bool contentExtensionReceivedAlert { false };

@interface ContentRuleListDelegate : NSObject <WKUIDelegate>
@end

@implementation ContentRuleListDelegate

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    switch (contentExtensionAlertCount++) {
    case 0:
        // Default behavior.
        EXPECT_STREQ("content blockers enabled", message.UTF8String);
        break;
    case 1:
        // After having removed the content RuleList.
        EXPECT_STREQ("content blockers disabled", message.UTF8String);
        break;
    default:
        EXPECT_TRUE(false);
    }
    contentExtensionReceivedAlert = true;
    completionHandler();
}

@end

TEST_F(WKContentRuleListStoreTest, AddRemove)
{
    [[WKContentRuleListStore defaultStore] _removeAllContentRuleLists];

    __block bool doneCompiling = false;
    NSString* contentBlocker = @"[{\"action\":{\"type\":\"css-display-none\",\"selector\":\".hidden\"},\"trigger\":{\"url-filter\":\".*\"}}]";
    __block RetainPtr<WKContentRuleList> ruleList;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"TestAddRemove" encodedContentRuleList:contentBlocker completionHandler:^(WKContentRuleList *compiledRuleList, NSError *error) {
        EXPECT_TRUE(error == nil);
        ruleList = compiledRuleList;
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
    EXPECT_NOT_NULL(ruleList);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addContentRuleList:ruleList.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr delegate = adoptNS([[ContentRuleListDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"contentBlockerCheck" withExtension:@"html"]];
    contentExtensionAlertCount = 0;
    contentExtensionReceivedAlert = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&contentExtensionReceivedAlert);

    [[configuration userContentController] removeContentRuleList:ruleList.get()];

    contentExtensionReceivedAlert = false;
    [webView reload];
    TestWebKitAPI::Util::run(&contentExtensionReceivedAlert);
}

#if PLATFORM(IOS_FAMILY)
TEST_F(WKContentRuleListStoreTest, UnsafeMMap)
{
    RetainPtr<NSString> tempDir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"UnsafeMMapTest"];
    RetainPtr<WKContentRuleListStore> store = [WKContentRuleListStore storeWithURL:[NSURL fileURLWithPath:tempDir.get() isDirectory:YES]];
    static NSString *compiledIdentifier = @"CompiledRuleList";
    static NSString *copiedIdentifier = @"CopiedRuleList";
    static NSString *ruleListSourceString = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"blockedsubresource\"}}]";
    RetainPtr<NSString> compiledFilePath = [tempDir stringByAppendingPathComponent:@"ContentRuleList-CompiledRuleList"];
    RetainPtr<NSString> copiedFilePath = [tempDir stringByAppendingPathComponent:@"ContentRuleList-CopiedRuleList"];

    __block bool doneCompiling = false;
    [store compileContentRuleListForIdentifier:compiledIdentifier encodedContentRuleList:ruleListSourceString completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    auto hasCompleteProtection = [] (const RetainPtr<NSString>& path) {
        NSError *error = nil;
        NSDictionary<NSFileAttributeKey, id> *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:path.get() error:&error];
        EXPECT_NULL(error);
        return [[attributes objectForKey:NSFileProtectionKey] isEqualToString:NSFileProtectionComplete];
    };
    
    NSError *error = nil;
    [[NSFileManager defaultManager] copyItemAtPath:compiledFilePath.get() toPath:copiedFilePath.get() error:&error];
    EXPECT_NULL(error);
    EXPECT_FALSE(hasCompleteProtection(copiedFilePath));
    [[NSFileManager defaultManager] setAttributes:@{ NSFileProtectionKey: NSFileProtectionComplete } ofItemAtPath:copiedFilePath.get() error:&error];
    EXPECT_NULL(error);
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    EXPECT_TRUE(hasCompleteProtection(copiedFilePath));
#endif

    __block bool doneLookingUp = false;
    [store lookUpContentRuleListForIdentifier:copiedIdentifier completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);
        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
    EXPECT_FALSE(hasCompleteProtection(copiedFilePath));
    
    __block bool doneRemoving = false;
    [store removeContentRuleListForIdentifier:compiledIdentifier completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        [store removeContentRuleListForIdentifier:copiedIdentifier completionHandler:^(NSError *error) {
            EXPECT_NULL(error);
            doneRemoving = true;
        }];
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}
#endif // PLATFORM(IOS_FAMILY)

static RetainPtr<WKContentRuleList> compileContentRuleList(const char* json, NSString *identifier = @"testidentifier")
{
    if ([identifier isEqualToString:@"testidentifier"])
        [[WKContentRuleListStore defaultStore] _removeAllContentRuleLists];

    __block RetainPtr<WKContentRuleList> list;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:identifier encodedContentRuleList:[NSString stringWithFormat:@"%s", json] completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NULL(error);
        list = filter;
    }];
    while (!list)
        TestWebKitAPI::Util::spinRunLoop();

    return list;
}

static RetainPtr<TestNavigationDelegate> navigationDelegateAllowingActiveActionsOnTestHost()
{
    static RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._activeContentRuleListActionPatterns = @{
            @"testidentifier": [NSSet setWithObject:@"testscheme://testhost/*"],
            @"testidentifier2": [NSSet setWithObject:@"testscheme://testhost/*"],
        };
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    return delegate;
}

static void checkURLs(const Vector<String>& actual, const Vector<String>& expected)
{
    EXPECT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < std::min(actual.size(), expected.size()); i++)
        EXPECT_WK_STREQ(actual[i], expected[i]);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeaders)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "set",
                "header": "testkey",
                "value": "testvalue"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 3, "request-headers": [ {
                "operation": "remove",
                "header": "Accept"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 4, "request-headers": [ {
                "operation": "append",
                "header": "Content-Language",
                "value": "de"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    __block bool receivedAllRequests { false };
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        NSDictionary<NSString *, NSString *> *fields = [task.request allHTTPHeaderFields];
        if ([path isEqualToString:@"/main.html"]) {
            EXPECT_WK_STREQ(fields[@"Content-Language"], "de");
            EXPECT_WK_STREQ(fields[@"testkey"], "testvalue");
            EXPECT_NULL(fields[@"Accept"]);
            return respond(task, "<iframe src='//otherhost/frame.html'></iframe>");
        }
        if ([path isEqualToString:@"/frame.html"]) {
            EXPECT_NULL(fields[@"Content-Language"]);
            EXPECT_NULL(fields[@"testkey"]);
            EXPECT_NOT_NULL(fields[@"Accept"]);
            return respond(task, "<script>fetch('//testhost/fetch.txt',{headers: {'Content-Language': 'en'}})</script>");
        }
        if ([path isEqualToString:@"/fetch.txt"]) {
            EXPECT_WK_STREQ(fields[@"Content-Language"], "en, de");
            EXPECT_WK_STREQ(fields[@"testkey"], "testvalue");
            EXPECT_NULL(fields[@"Accept"]);
            receivedAllRequests = true;
            return respond(task, "");
        }
        ASSERT_NOT_REACHED();
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto delegate = navigationDelegateAllowingActiveActionsOnTestHost().unsafeGet();
    webView.get().navigationDelegate = delegate;
    __block bool receivedActionNotification { false };
    __block Vector<String> urls;
    delegate.contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        urls.append(url.absoluteString);
        EXPECT_TRUE(action.modifiedHeaders);
        receivedActionNotification = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedAllRequests);
    TestWebKitAPI::Util::run(&receivedActionNotification);
    checkURLs(urls, {
        "testscheme://testhost/main.html"_s,
        "testscheme://testhost/fetch.txt"_s
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersWithCompetingRulesWhereAppendWins)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 4, "request-headers": [ {
                "operation": "append",
                "header": "Via",
                "value": "1.1 proxy-a"
            }, {
                "operation": "append",
                "header": "Forwarded",
                "value": "for=192.0.2.1"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 3, "request-headers": [ {
                "operation": "set",
                "header": "Via",
                "value": "should-not-apply"
            }, {
                "operation": "set",
                "header": "Forwarded",
                "value": "should-not-apply"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "remove",
                "header": "Via"
            }, {
                "operation": "remove",
                "header": "Forwarded"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Via",
                "value": "1.1 proxy-b"
            }, {
                "operation": "append",
                "header": "Forwarded",
                "value": "for=192.0.2.2"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }]
    )JSON");

    __block bool receivedAllRequests { false };
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        NSDictionary<NSString *, NSString *> *fields = [task.request allHTTPHeaderFields];
        if ([path isEqualToString:@"/main.html"]) {
            EXPECT_WK_STREQ(fields[@"Via"], "1.1 proxy-a, 1.1 proxy-b");
            EXPECT_WK_STREQ(fields[@"Forwarded"], "for=192.0.2.1, for=192.0.2.2");
            receivedAllRequests = true;
            return respond(task, "");
        }
        ASSERT_NOT_REACHED();
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto delegate = navigationDelegateAllowingActiveActionsOnTestHost().unsafeGet();
    webView.get().navigationDelegate = delegate;
    __block bool receivedActionNotification { false };
    __block Vector<String> urls;
    delegate.contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        urls.append(url.absoluteString);
        EXPECT_TRUE(action.modifiedHeaders);
        receivedActionNotification = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedAllRequests);
    TestWebKitAPI::Util::run(&receivedActionNotification);
    checkURLs(urls, {
        "testscheme://testhost/main.html"_s,
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersWithCompetingRulesWhereSetWins)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 10, "request-headers": [ {
                "operation": "append",
                "header": "Via",
                "value": "1.1 proxy-b"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 20, "request-headers": [ {
                "operation": "set",
                "header": "Via",
                "value": "should-not-apply"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 40, "request-headers": [ {
                "operation": "remove",
                "header": "Via"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 80, "request-headers": [ {
                "operation": "set",
                "header": "Via",
                "value": "1.1 proxy-a"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 3, "request-headers": [ {
                "operation": "set",
                "header": "Forwarded",
                "value": "for=192.0.2.1"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "append",
                "header": "Forwarded",
                "value": "for=192.0.2.2"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Forwarded",
                "value": "for=192.0.2.3"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }]
    )JSON");

    __block bool receivedAllRequests { false };
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        NSDictionary<NSString *, NSString *> *fields = [task.request allHTTPHeaderFields];
        if ([path isEqualToString:@"/main.html"]) {
            EXPECT_WK_STREQ(fields[@"Via"], "1.1 proxy-a, 1.1 proxy-b");
            EXPECT_WK_STREQ(fields[@"Forwarded"], "for=192.0.2.1, for=192.0.2.2, for=192.0.2.3");
            receivedAllRequests = true;
            return respond(task, "");
        }
        ASSERT_NOT_REACHED();
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto delegate = navigationDelegateAllowingActiveActionsOnTestHost().unsafeGet();
    webView.get().navigationDelegate = delegate;
    __block bool receivedActionNotification { false };
    __block Vector<String> urls;
    delegate.contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        urls.append(url.absoluteString);
        EXPECT_TRUE(action.modifiedHeaders);
        receivedActionNotification = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedAllRequests);
    TestWebKitAPI::Util::run(&receivedActionNotification);
    checkURLs(urls, {
        "testscheme://testhost/main.html"_s,
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersWithCompetingRulesWhereRemoveWins)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 5, "request-headers": [ {
                "operation": "append",
                "header": "Via",
                "value": "second-should-not-apply"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 7, "request-headers": [ {
                "operation": "set",
                "header": "Via",
                "value": "should-not-apply"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 9, "request-headers": [ {
                "operation": "remove",
                "header": "Via"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 3, "request-headers": [ {
                "operation": "remove",
                "header": "Via"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }]
    )JSON");

    __block bool receivedAllRequests { false };
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        NSDictionary<NSString *, NSString *> *fields = [task.request allHTTPHeaderFields];
        if ([path isEqualToString:@"/main.html"]) {
            EXPECT_WK_STREQ(fields[@"Via"], "");
            receivedAllRequests = true;
            return respond(task, "");
        }
        ASSERT_NOT_REACHED();
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto delegate = navigationDelegateAllowingActiveActionsOnTestHost().unsafeGet();
    webView.get().navigationDelegate = delegate;
    __block bool receivedActionNotification { false };
    __block Vector<String> urls;
    delegate.contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        urls.append(url.absoluteString);
        EXPECT_TRUE(action.modifiedHeaders);
        receivedActionNotification = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedAllRequests);
    TestWebKitAPI::Util::run(&receivedActionNotification);
    checkURLs(urls, {
        "testscheme://testhost/main.html"_s,
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersWithMultipleRuleLists)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 4, "request-headers": [ {
                "operation": "append",
                "header": "Via",
                "value": "1.1 proxy-b"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 8, "request-headers": [ {
                "operation": "set",
                "header": "Via",
                "value": "1.0 proxy-a"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON", @"testidentifier");

    auto secondList = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Via",
                "value": "1.1 proxy-c"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "remove",
                "header": "Via"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }]
    )JSON", @"testidentifier2");

    __block bool receivedAllRequests { false };
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        NSDictionary<NSString *, NSString *> *fields = [task.request allHTTPHeaderFields];
        if ([path isEqualToString:@"/main.html"]) {
            EXPECT_WK_STREQ(fields[@"Via"], "1.0 proxy-a, 1.1 proxy-b, 1.1 proxy-c");
            receivedAllRequests = true;
            return respond(task, "");
        }
        ASSERT_NOT_REACHED();
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [[configuration userContentController] addContentRuleList:secondList.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto delegate = navigationDelegateAllowingActiveActionsOnTestHost().unsafeGet();
    webView.get().navigationDelegate = delegate;
    __block bool receivedActionNotification { false };
    __block Vector<String> urls;
    delegate.contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        urls.append(url.absoluteString);
        EXPECT_TRUE(action.modifiedHeaders);
        receivedActionNotification = true;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedAllRequests);
    TestWebKitAPI::Util::run(&receivedActionNotification);
    checkURLs(urls, {
        "testscheme://testhost/main.html"_s,
        "testscheme://testhost/main.html"_s,
    });
}

TEST_F(WKContentRuleListStoreTest, Redirect)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "redirect", "redirect": {
                "url": "othertestscheme://not-testhost/1-redirected.txt"
            } },
            "trigger": { "url-filter": "1.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "transform": {
                "port": "123",
                "ignored-comment": "123 is a forbidden port, so this URL is not seen by the WKURLSchemeHandler."
            } } },
            "trigger": { "url-filter": "2.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "transform": {
                "host": "newhost",
                "username": "newusername",
                "port": "443"
            } } },
            "trigger": { "url-filter": "3.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "transform": {
                "port": "",
                "scheme": "othertestscheme",
                "query": "?key=value"
            } } },
            "trigger": { "url-filter": "4.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "transform": {
                "password": "testpassword",
                "query": "",
                "fragment": "#fragment-to-be-added"
            } } },
            "trigger": { "url-filter": "5.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "transform": {
                "password": "testpassword",
                "fragment": ""
            } } },
            "trigger": { "url-filter": "6.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "transform": {
                "query": "?",
                "fragment": "#"
            } } },
            "trigger": { "url-filter": "7.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "transform": {
                "query-transform": {
                    "remove-parameters": [ "parameter-to-remove" ],
                    "add-or-replace-parameters": [
                        { "key": "key-to-add", "value": "value-to-add" },
                        { "key": "key-to-replace-only", "value": "value-to-replace-only", "replace-only": true },
                        { "key": "key-to-replace-only-but-not-found", "value": "value-to-replace-only-but-not-found", "replace-only": true }
                    ]
                }
            } } },
            "trigger": { "url-filter": "8.txt" }
        }, {
            "action": { "type": "redirect", "redirect": { "regex-substitution": "testscheme://replaced-by-regex/\\1.txt" } },
            "trigger": { "url-filter": "(9).txt" }
        } ]
    )JSON");

    __block bool receivedAllRequests { false };
    __block RetainPtr urls = adoptNS([NSMutableArray new]);
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSURL *url = task.request.URL;
        [urls addObject:url];
        NSString *path = url.path;
        if ([path isEqualToString:@"/main.html"]) {
            return respond(task, R"HTML(
                <script>
                    async function fetchSubresources()
                    {
                        try { await fetch('1.txt') } catch(e) { };
                        try { await fetch('//not-testhost-should-not-trigger-action/2.txt', {mode:'no-cors'}) } catch(e) { };
                        try { await fetch('//testhost/2.txt') } catch(e) { };
                        try { await fetch('/3.txt?query-should-not-be-removed') } catch(e) { };
                        try { await fetch('//testhost:80/4.txt#fragment-should-not-be-removed') } catch(e) { };
                        try { await fetch('5.txt?query-should-be-removed') } catch(e) { };
                        try { await fetch('6.txt#fragment-should-be-removed') } catch(e) { };
                        try { await fetch('7.txt') } catch(e) { };
                        try { await fetch('8.txt?key-to-replace-only=pre-replacement-value') } catch(e) { };
                        try { await fetch('9.txt') } catch(e) { };
                    }
                    fetchSubresources();
                </script>
            )HTML");
        }
        if ([path isEqualToString:@"/9.txt"])
            receivedAllRequests = true;
        respond(task, "");
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"othertestscheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto delegate = navigationDelegateAllowingActiveActionsOnTestHost().unsafeGet();
    webView.get().navigationDelegate = delegate;
    __block bool receivedActionNotification { false };
    __block Vector<String> urlsFromCallback;
    delegate.contentRuleListPerformedAction = ^(WKWebView *, NSString *identifier, _WKContentRuleListAction *action, NSURL *url) {
        urlsFromCallback.append(url.absoluteString);
        EXPECT_TRUE(action.redirected);
        receivedActionNotification = true;
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedAllRequests);
    TestWebKitAPI::Util::run(&receivedActionNotification);

    Vector<String> expectedRequestedURLs {
        "testscheme://testhost/main.html"_s,
        "othertestscheme://not-testhost/1-redirected.txt"_s,
        "testscheme://not-testhost-should-not-trigger-action/2.txt"_s,
        "testscheme://newusername@newhost:443/3.txt?query-should-not-be-removed"_s,
        "othertestscheme://testhost/4.txt?key=value#fragment-should-not-be-removed"_s,
        "testscheme://:testpassword@testhost/5.txt#fragment-to-be-added"_s,
        "testscheme://:testpassword@testhost/6.txt"_s,
        "testscheme://testhost/7.txt?#"_s,
        "testscheme://testhost/8.txt?key-to-replace-only=value-to-replace-only&key-to-add=value-to-add"_s,
        "testscheme://replaced-by-regex/9.txt"_s,
    };
    EXPECT_EQ(expectedRequestedURLs.size(), [urls count]);
    for (size_t i = 0; i < expectedRequestedURLs.size(); i++)
        EXPECT_WK_STREQ(expectedRequestedURLs[i], [[urls objectAtIndex:i] absoluteString]);

    expectedRequestedURLs.removeAt(0);
    expectedRequestedURLs[1] = "testscheme://testhost:123/2.txt"_s;
    checkURLs(urlsFromCallback, expectedRequestedURLs);
}

TEST_F(WKContentRuleListStoreTest, MainResourceCrossOriginRedirect)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/1.txt"_s, { "<script>alert('this should never load')</script>"_s } },
        { "/1.replaced"_s, { "<script>document.cookie = 'key=value'; alert('loaded replacement successfully: ' + document.cookie)</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "redirect", "redirect": { "regex-substitution": "https://webkit.org/\\1.replaced" } },
            "trigger": { "url-filter": "(1).txt" }
        } ]
    )JSON");

    auto configuration = server.httpsProxyConfiguration();
    [[configuration userContentController] addContentRuleList:list.get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._activeContentRuleListActionPatterns = @{
            @"testidentifier": [NSSet setWithObject:@"https://example.com/*"]
        };
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [delegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = delegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/1.txt"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded replacement successfully: key=value");
}

TEST_F(WKContentRuleListStoreTest, MainResourceSameOriginRedirect)
{
    using namespace TestWebKitAPI;

    HTTPServer server({
        { "/redirect?"_s, { "<script>addEventListener('pageshow', () => { alert('redirected successfully') });</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "redirect", "redirect": { "transform": { "query-transform": { "remove-parameters": [ "ved" ] } } } },
            "trigger": { "url-filter": "redirect" }
        } ]
    )JSON");

    auto configuration = server.httpsProxyConfiguration();
    [configuration.userContentController addContentRuleList:list.get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._activeContentRuleListActionPatterns = @{
            @"testidentifier": [NSSet setWithObject:@"https://example.com/*"]
        };
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [delegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = delegate.get();

    auto originalURL = [NSURL URLWithString:@"https://example.com/redirect?ved=123"];
    [webView loadRequest:[NSURLRequest requestWithURL:originalURL]];

    // Verify that the parameters were removed from the URL and the page loads.
    EXPECT_WK_STREQ([webView _test_waitForAlert], "redirected successfully");
}

TEST_F(WKContentRuleListStoreTest, MainResourceCrossOriginRedirectFromLoadedPageWithoutActivePatterns)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/example"_s, { "<script>addEventListener('pageshow', () => { alert('original page loaded') });</script>"_s } },
        { "/after_redirect"_s, { "<script>alert('loaded replacement successfully: ' + window.location.href)</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "redirect", "redirect": { "regex-substitution": "https://apple.com/after_redirect" } },
            "trigger": { "url-filter": "before_redirect" }
        } ]
    )JSON");

    auto configuration = server.httpsProxyConfiguration();
    [[configuration userContentController] addContentRuleList:list.get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._activeContentRuleListActionPatterns = @{
            @"testidentifier": [NSSet setWithObject:@"https://webkit.org/*"]
        };
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [delegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = delegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "original page loaded");
    [webView evaluateJavaScript:@"window.location = 'https://webkit.org/before_redirect'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded replacement successfully: https://apple.com/after_redirect");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "original page loaded");
}

static RetainPtr<TestNavigationDelegate> navigationDelegateAllowingActiveActionsOnLoopback()
{
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._activeContentRuleListActionPatterns = @{
            @"testidentifier": [NSSet setWithObject:@"*://127.0.0.1/*"],
        };
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    return delegate;
}

using RequestHeadersCheck = void (^)(NSDictionary<NSString *, NSString *> *);

static void loadMainAndCheckRequestHeaders(WKContentRuleList *list, RequestHeadersCheck check)
{
    __block bool receivedRequest { false };

    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        check([task.request allHTTPHeaderFields]);
        receivedRequest = true;

        respond(task, "");
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    RetainPtr delegate = navigationDelegateAllowingActiveActionsOnTestHost();
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedRequest);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendCookieUsesSemicolon)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 3, "request-headers": [ {
                "operation": "set",
                "header": "Cookie",
                "value": "a=1"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "append",
                "header": "Cookie",
                "value": "b=2"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "cookie",
                "value": "c=3"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        EXPECT_WK_STREQ(fields[@"Cookie"], "a=1; b=2; c=3");
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendToMissingHeaderSetsValue)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Cookie",
                "value": "a=1"
            }, {
                "operation": "append",
                "header": "Via",
                "value": "1.1 proxy-a"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        EXPECT_WK_STREQ(fields[@"Cookie"], "a=1");
        EXPECT_WK_STREQ(fields[@"Via"], "1.1 proxy-a");
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendHeaderNameIsCaseInsensitive)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "set",
                "header": "Via",
                "value": "1.1 proxy-a"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "VIA",
                "value": "1.1 proxy-b"
            }, {
                "operation": "append",
                "header": "X-Forwarded-For",
                "value": "192.0.2.1"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        EXPECT_WK_STREQ(fields[@"Via"], "1.1 proxy-a, 1.1 proxy-b");
        EXPECT_WK_STREQ(fields[@"X-Forwarded-For"], "192.0.2.1");
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendAcceptLanguageKeepsDefaultLanguages)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Accept-Language",
                "value": "de-DE,de;q=0.8"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        NSString *value = fields[@"Accept-Language"];

        EXPECT_TRUE([value hasSuffix:@"de-DE,de;q=0.8"]);

        // The user's preferred languages stay in front of the appended value.
        if ([NSLocale preferredLanguages].count)
            EXPECT_TRUE([value hasSuffix:@", de-DE,de;q=0.8"]);
    });
}

static void checkAppendedAcceptEncoding(TestWebKitAPI::HTTPServer::Protocol protocol, const String& expectedHeaderLine)
{
    using namespace TestWebKitAPI;

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Accept-Encoding",
                "value": "zstd"
            } ] },
            "trigger": { "url-filter": "127.0.0.1" }
        } ]
    )JSON");

    bool receivedRequest { false };
    bool hadExpectedHeader { false };

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        hadExpectedHeader = String::fromUTF8(request.span()).contains(expectedHeaderLine);
        receivedRequest = true;

        co_await connection.awaitableSend(HTTPResponse("hi"_s).serialize());
    }, protocol);

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    RetainPtr delegate = navigationDelegateAllowingActiveActionsOnLoopback();
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&receivedRequest);

    EXPECT_TRUE(hadExpectedHeader);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendAcceptEncodingOverHTTPKeepsDefaultEncodings)
{
    checkAppendedAcceptEncoding(TestWebKitAPI::HTTPServer::Protocol::Http, "Accept-Encoding: gzip, deflate, zstd\r\n"_s);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendAcceptEncodingOverHTTPSKeepsDefaultEncodings)
{
    checkAppendedAcceptEncoding(TestWebKitAPI::HTTPServer::Protocol::Https, "Accept-Encoding: gzip, deflate, br, zstd\r\n"_s);
}

static NSString *userAgentHeaderValue(const Vector<char>& request)
{
    RetainPtr requestString = String::fromUTF8(request.span()).createNSString();
    __block RetainPtr<NSString> userAgent;

    [requestString enumerateLinesUsingBlock:^(NSString *line, BOOL *stop) {
        if (![line hasPrefix:@"User-Agent: "])
            return;

        userAgent = [line substringFromIndex:@"User-Agent: ".length];
        *stop = YES;
    }];

    return userAgent.autorelease();
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendUserAgent)
{
    using namespace TestWebKitAPI;

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "User-Agent",
                "value": "AppendedByRule"
            } ] },
            "trigger": { "url-filter": "127.0.0.1" }
        } ]
    )JSON");

    bool receivedRequest { false };
    RetainPtr<NSString> userAgent;

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        userAgent = userAgentHeaderValue(request);
        receivedRequest = true;

        co_await connection.awaitableSend(HTTPResponse("hi"_s).serialize());
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    RetainPtr delegate = navigationDelegateAllowingActiveActionsOnLoopback();
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&receivedRequest);

    // The default user agent stays in front of the appended value.
    EXPECT_WK_STREQ(userAgent.get(), [webView.get()._userAgent stringByAppendingString:@" AppendedByRule"]);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendUserAgentOnHyperlinkAuditingPing)
{
    using namespace TestWebKitAPI;

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "User-Agent",
                "value": "AppendedByRule"
            } ] },
            "trigger": { "url-filter": "/ping" }
        } ]
    )JSON");

    bool receivedPing { false };
    RetainPtr<NSString> pingUserAgent;
    RetainPtr<NSString> pageUserAgent;

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);

            if (path == "/"_s) {
                pageUserAgent = userAgentHeaderValue(request);
                co_await connection.awaitableSend(HTTPResponse("<a id='link' href='/destination' ping='/ping'>link</a>"_s).serialize());
            } else if (path == "/ping"_s) {
                pingUserAgent = userAgentHeaderValue(request);
                receivedPing = true;
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
            } else
                co_await connection.awaitableSend(HTTPResponse("destination"_s).serialize());
        }
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    RetainPtr delegate = navigationDelegateAllowingActiveActionsOnLoopback();
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:server.request()];
    [delegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedPing);

    EXPECT_WK_STREQ(pageUserAgent.get(), webView.get()._userAgent);
    EXPECT_WK_STREQ(pingUserAgent.get(), [webView.get()._userAgent stringByAppendingString:@" AppendedByRule"]);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendUserAgentOnFetchAndXHR)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "User-Agent",
                "value": "AppendedByRule"
            } ] },
            "trigger": { "url-filter": "testhost/.*txt" }
        } ]
    )JSON");

    __block bool receivedAllRequests { false };
    __block RetainPtr<NSString> fetchUserAgent;
    __block RetainPtr<NSString> xhrUserAgent;

    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        NSString *userAgent = task.request.allHTTPHeaderFields[@"User-Agent"];

        if ([path isEqualToString:@"/main.html"])
            return respond(task, "<script>fetch('/fetch.txt').then(() => { let request = new XMLHttpRequest(); request.open('GET', '/xhr.txt'); request.send(); })</script>");

        if ([path isEqualToString:@"/fetch.txt"]) {
            fetchUserAgent = userAgent;

            return respond(task, "");
        }

        if ([path isEqualToString:@"/xhr.txt"]) {
            xhrUserAgent = userAgent;
            receivedAllRequests = true;

            return respond(task, "");
        }

        ASSERT_NOT_REACHED();
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    RetainPtr delegate = navigationDelegateAllowingActiveActionsOnTestHost();
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    TestWebKitAPI::Util::run(&receivedAllRequests);

    RetainPtr expectedUserAgent = [webView.get()._userAgent stringByAppendingString:@" AppendedByRule"];
    EXPECT_WK_STREQ(fetchUserAgent.get(), expectedUserAgent.get());
    EXPECT_WK_STREQ(xhrUserAgent.get(), expectedUserAgent.get());
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendAcceptLanguageFromMultipleRules)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "append",
                "header": "Accept-Language",
                "value": "de"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Accept-Language",
                "value": "fr"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    // The default languages are only added once, in front of the first appended value.
    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        NSString *value = fields[@"Accept-Language"];

        EXPECT_TRUE([value hasSuffix:@", de, fr"]);
        EXPECT_GT(value.length, @", de, fr".length);
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendAcceptLanguageAfterSetDoesNotAddDefault)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "set",
                "header": "Accept-Language",
                "value": "fr"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Accept-Language",
                "value": "de"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        EXPECT_WK_STREQ(fields[@"Accept-Language"], "fr, de");
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendAcceptLanguageAfterRemoveIsIgnored)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "remove",
                "header": "Accept-Language"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Accept-Language",
                "value": "de"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        EXPECT_NULL(fields[@"Accept-Language"]);
    });
}

TEST_F(WKContentRuleListStoreTest, BlockedHyperlinkAuditingPingIsNotSent)
{
    using namespace TestWebKitAPI;

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "block" },
            "trigger": { "url-filter": "/ping" }
        } ]
    )JSON");

    bool receivedPing { false };
    bool receivedDestination { false };

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);

            if (path == "/"_s)
                co_await connection.awaitableSend(HTTPResponse("<a id='link' href='/destination' ping='/ping'>link</a>"_s).serialize());
            else if (path == "/ping"_s) {
                receivedPing = true;

                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
            } else {
                receivedDestination = true;

                co_await connection.awaitableSend(HTTPResponse("destination"_s).serialize());
            }
        }
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    RetainPtr delegate = navigationDelegateAllowingActiveActionsOnLoopback();
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:server.request()];
    [delegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedDestination);
    TestWebKitAPI::Util::runFor(0.5_s);

    EXPECT_FALSE(receivedPing);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendUserAgentOnContentSecurityPolicyReport)
{
    using namespace TestWebKitAPI;

    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "User-Agent",
                "value": "AppendedByRule"
            } ] },
            "trigger": { "url-filter": "/report" }
        } ]
    )JSON");

    bool receivedReport { false };
    RetainPtr<NSString> reportUserAgent;

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);

            if (path == "/report"_s) {
                reportUserAgent = userAgentHeaderValue(request);
                receivedReport = true;

                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
            } else
                co_await connection.awaitableSend(HTTPResponse({ { "Content-Security-Policy"_s, "script-src 'none'; report-uri /report"_s } }, "<script>window.ran = true</script>"_s).serialize());
        }
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    RetainPtr delegate = navigationDelegateAllowingActiveActionsOnLoopback();
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&receivedReport);

    EXPECT_WK_STREQ(reportUserAgent.get(), [webView.get()._userAgent stringByAppendingString:@" AppendedByRule"]);
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendTrailerHasNoDelimiter)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 2, "request-headers": [ {
                "operation": "set",
                "header": "Trailer",
                "value": "Expires"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        }, {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Trailer",
                "value": ", ETag"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    // Trailer uses an empty delimiter, so the appended value supplies its own separator.
    loadMainAndCheckRequestHeaders(list.get(), ^(NSDictionary<NSString *, NSString *> *fields) {
        EXPECT_WK_STREQ(fields[@"Trailer"], "Expires, ETag");
    });
}

TEST_F(WKContentRuleListStoreTest, ModifyHeadersAppendRequestHeaderAllowlist)
{
    auto compile = [](const char* json) {
        __block RetainPtr<NSError> compileError;
        __block bool done { false };

        [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"testidentifier" encodedContentRuleList:[NSString stringWithFormat:@"%s", json] completionHandler:^(WKContentRuleList *, NSError *error) {
            compileError = error;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        return compileError;
    };

    // Single-valued request headers can't be appended to.
    auto requestError = compile(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "request-headers": [ {
                "operation": "append",
                "header": "Content-Type",
                "value": "text/plain"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    EXPECT_NOT_NULL(requestError.get());
    EXPECT_EQ(requestError.get().code, WKErrorContentRuleListStoreCompileFailed);

    // Response headers have no such restriction.
    auto responseError = compile(R"JSON(
        [ {
            "action": { "type": "modify-headers", "priority": 1, "response-headers": [ {
                "operation": "append",
                "header": "Content-Type",
                "value": "text/plain"
            } ] },
            "trigger": { "url-filter": "testscheme" }
        } ]
    )JSON");

    EXPECT_NULL(responseError.get());
}

TEST_F(WKContentRuleListStoreTest, NullPatternSet)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "redirect", "redirect": {
                "url": "testscheme://testhost/main-redirected.html"
            } },
            "trigger": { "url-filter": "main.html" }
        } ]
    )JSON");

    __block std::optional<bool> redirectedResult;
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        if ([path isEqualToString:@"/main.html"])
            redirectedResult = false;
        else {
            EXPECT_WK_STREQ(path, "/main-redirected.html");
            redirectedResult = true;
        }
        respond(task, "");
    };

    __block enum class DelegateAction : uint8_t {
        AllowAll,
        AllowNone,
        AllowTestHost,
    } delegateAction;
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        EXPECT_NOT_NULL(preferences._activeContentRuleListActionPatterns);
        EXPECT_EQ(preferences._activeContentRuleListActionPatterns.count, 0u);
        switch (delegateAction) {
        case DelegateAction::AllowAll:
            preferences._activeContentRuleListActionPatterns = [NSDictionary dictionaryWithObject:[NSSet setWithObject:@"testscheme://*/*"] forKey:@"testidentifier"];
            break;
        case DelegateAction::AllowNone:
            preferences._activeContentRuleListActionPatterns = [NSDictionary dictionary];
            break;
        case DelegateAction::AllowTestHost:
            preferences._activeContentRuleListActionPatterns = [NSDictionary dictionaryWithObject:[NSSet setWithObject:@"testscheme://testhost/*"] forKey:@"testidentifier"];
            break;
        }
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] addContentRuleList:list.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    webView.get().navigationDelegate = delegate.get();

    auto getRedirectResult = ^(DelegateAction action) {
        delegateAction = action;
        redirectedResult = std::nullopt;
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
        while (!redirectedResult)
            TestWebKitAPI::Util::spinRunLoop();
        return *redirectedResult;
    };
    EXPECT_TRUE(getRedirectResult(DelegateAction::AllowAll));
    EXPECT_FALSE(getRedirectResult(DelegateAction::AllowNone));
    EXPECT_TRUE(getRedirectResult(DelegateAction::AllowTestHost));
}

TEST_F(WKContentRuleListStoreTest, ExtensionPath)
{
    auto list = compileContentRuleList(R"JSON(
        [ {
            "action": { "type": "redirect", "redirect": {
                "extension-path": "/redirected-to-extension?no-query#no-fragment"
            } },
            "trigger": { "url-filter": "main.html" }
        } ]
    )JSON");

    __block RetainPtr<NSURL> redirectedURL;
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id <WKURLSchemeTask> task) {
        redirectedURL = task.request.URL;
        respond(task, "");
    };

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._activeContentRuleListActionPatterns = [NSDictionary dictionaryWithObject:[NSSet setWithObject:@"testscheme://testhost/*"] forKey:@"testidentifier"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration userContentController] _addContentRuleList:list.get() extensionBaseURL:[NSURL URLWithString:@"extension-scheme://extension-host/"]];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testscheme"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"extension-scheme"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testscheme://testhost/main.html"]]];
    while (!redirectedURL)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_WK_STREQ([redirectedURL absoluteString], "extension-scheme://extension-host/redirected-to-extension%3Fno-query%23no-fragment");
}

TEST_F(WKContentRuleListStoreTest, NonASCIIEscaped)
{
    NSString *source = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"t,[{`\\\\\\\\\\\\20442=OvI6\",\"\":[\"\"]}}]";
    __block bool done { false };
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"test" encodedContentRuleList:source completionHandler:^(WKContentRuleList *filter, NSError *error) {
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

static RetainPtr<NSString> createMixedContentAutoUpgradeJsonRuleList()
{
    auto* mixedContentAutoupgrade = @{
        @"trigger" : @{
            @"url-filter" : @"http://.*",
            @"if-top-url" : @[ @"https://.*" ],
            @"resource-type" : @[
                // Only upgrade image and media (i.e. audio and video) per
                // https://www.w3.org/TR/mixed-content/#upgrade-algorithm
                @"image", @"media"
            ],
        },
        @"action" : @{
            @"type" : @"make-https",
        },
    };

    RetainPtr jsonData = [NSJSONSerialization dataWithJSONObject:@[ mixedContentAutoupgrade ] options:NSJSONWritingPrettyPrinted error:nil];
    return adoptNS([[NSString alloc] initWithData:jsonData.get() encoding:NSUTF8StringEncoding]);
}

TEST_F(WKContentRuleListStoreTest, ConcurrentCompilationsSingleStore)
{
    RetainPtr string = createMixedContentAutoUpgradeJsonRuleList();
    static constexpr unsigned totalRequests = 30;
    __block unsigned finishedCount = 0;
    for (unsigned i = 0; i < totalRequests; ++i) {
        [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"test" encodedContentRuleList:string.get() completionHandler:^(WKContentRuleList *filter, NSError *error) {
            EXPECT_NOT_NULL(filter);
            EXPECT_NULL(error);
            ++finishedCount;
        }];
    }
    while (finishedCount < totalRequests)
        TestWebKitAPI::Util::spinRunLoop();
}

TEST_F(WKContentRuleListStoreTest, ConcurrentCompilationsMultipleStores)
{
    static constexpr unsigned totalRequests = 30;

    Vector<RetainPtr<WKContentRuleListStore>> stores;
    RetainPtr tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"ContentRuleListTest"] isDirectory:YES];
    for (unsigned i = 0; i < totalRequests; ++i)
        stores.append([WKContentRuleListStore storeWithURL:tempDir.get()]);

    RetainPtr string = createMixedContentAutoUpgradeJsonRuleList();
    __block unsigned finishedCount = 0;
    for (unsigned i = 0; i < totalRequests; ++i) {
        [stores[i] compileContentRuleListForIdentifier:@"test" encodedContentRuleList:string.get() completionHandler:^(WKContentRuleList *filter, NSError *error) {
            EXPECT_NOT_NULL(filter);
            EXPECT_NULL(error);
            ++finishedCount;
        }];
    }
    while (finishedCount < totalRequests)
        TestWebKitAPI::Util::spinRunLoop();
}
