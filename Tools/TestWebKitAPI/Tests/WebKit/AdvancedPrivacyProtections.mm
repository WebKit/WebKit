/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

#import "HTTPServer.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "UserInterfaceSwizzler.h"
#import <WebCore/LinkDecorationFilteringData.h>
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#endif

#import <pal/cocoa/WebPrivacySoftLink.h>

@interface WPLinkFilteringData (TestSupport)
- (instancetype)initWithQueryParameters:(NSArray<NSString *> *)queryParameters;
- (instancetype)initWithQueryParameters:(NSArray<NSString *> *)queryParameters domains:(NSArray<NSString *> *)domains paths:(NSArray<NSString *> *)paths;
@end

@interface TestWKWebView (TrackingPrevention)
- (id)callAsyncJavaScriptAndWait:(NSString *)script;
- (id)callAsyncJavaScriptAndWait:(NSString *)script arguments:(NSDictionary *)arguments;
@end

@implementation TestWKWebView (TrackingPrevention)

- (CGSize)computeScreenSizeForBindings
{
    NSArray<NSNumber *> *values = [self objectByEvaluatingJavaScript:@"[screen.width, screen.height]"];
    return CGSizeMake(values[0].doubleValue, values[1].doubleValue);
}

- (id)callAsyncJavaScriptAndWait:(NSString *)script
{
    return [self callAsyncJavaScriptAndWait:script arguments:@{ }];
}

- (id)callAsyncJavaScriptAndWait:(NSString *)script arguments:(NSDictionary *)arguments
{
    __block RetainPtr<id> evaluationResult;
    __block bool done = false;
    [self callAsyncJavaScript:script arguments:arguments inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_WK_STREQ(@"", [error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey]);
        EXPECT_WK_STREQ(@"", error.domain);
        EXPECT_EQ(0, error.code);
        evaluationResult = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return evaluationResult.autorelease();
}

@end

namespace TestWebKitAPI {

#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

static IMP makeQueryParameterRequestHandler(NSArray<NSString *> *parameters, NSArray<NSString *> *domains, NSArray<NSString *> *paths, bool& didHandleRequest)
{
    return imp_implementationWithBlock([&didHandleRequest, parameters = RetainPtr { parameters }, domains = RetainPtr { domains }, paths = RetainPtr { paths }](WPResources *, WPResourceRequestOptions *, void(^completion)(WPLinkFilteringData *, NSError *)) mutable {
        RunLoop::main().dispatch([&didHandleRequest, parameters = WTFMove(parameters), domains = WTFMove(domains), paths = WTFMove(paths), completion = makeBlockPtr(completion)]() mutable {
            auto data = adoptNS([PAL::allocWPLinkFilteringDataInstance() initWithQueryParameters:parameters.get()
#if defined(HAS_WEB_PRIVACY_LINK_FILTERING_RULE_PATH)
                domains:domains.get() paths:paths.get()
#endif
            ]);
            completion(data.get(), nil);
            didHandleRequest = true;
        });
    });
}

static IMP makeAllowedLinkFilteringDataRequestHandler(NSArray<NSString *> *parameters)
{
    return imp_implementationWithBlock([parameters = RetainPtr { parameters }](WPResources *, WPResourceRequestOptions *, void(^completion)(WPLinkFilteringData *, NSError *)) mutable {
        RunLoop::main().dispatch([parameters = WTFMove(parameters), completion = makeBlockPtr(completion)]() mutable {
            auto data = adoptNS([PAL::allocWPLinkFilteringDataInstance() initWithQueryParameters:parameters.get()]);
            completion(data.get(), nil);
        });
    });
}

class AllowedLinkFilteringDataRequestSwizzler {
    WTF_MAKE_NONCOPYABLE(AllowedLinkFilteringDataRequestSwizzler);
    WTF_MAKE_FAST_ALLOCATED;
public:
    AllowedLinkFilteringDataRequestSwizzler(NSArray<NSString *> *parameters)
    {
        m_swizzler = makeUnique<InstanceMethodSwizzler>(
            PAL::getWPResourcesClass(),
            @selector(requestAllowedLinkFilteringData:completionHandler:),
            makeAllowedLinkFilteringDataRequestHandler(parameters)
        );
    }

private:
    std::unique_ptr<InstanceMethodSwizzler> m_swizzler;
};

class QueryParameterRequestSwizzler {
    WTF_MAKE_NONCOPYABLE(QueryParameterRequestSwizzler);
    WTF_MAKE_FAST_ALLOCATED;
public:
    QueryParameterRequestSwizzler(NSArray<NSString *> *parameters, NSArray<NSString *> *domains, NSArray<NSString *> *paths)
    {
        update(parameters, domains, paths);
    }

    void update(NSArray<NSString *> *parameters, NSArray<NSString *> *domains, NSArray<NSString *> *paths)
    {
        m_didHandleRequest = false;
        // Ensure that the previous swizzler is destroyed before creating the new one.
        m_swizzler = nullptr;
        m_swizzler = makeUnique<InstanceMethodSwizzler>(PAL::getWPResourcesClass(), @selector(requestLinkFilteringData:completionHandler:), makeQueryParameterRequestHandler(parameters, domains, paths, m_didHandleRequest));
        [[NSNotificationCenter defaultCenter] postNotificationName:PAL::get_WebPrivacy_WPResourceDataChangedNotificationName() object:nil userInfo:@{ PAL::get_WebPrivacy_WPNotificationUserInfoResourceTypeKey() : @(WPResourceTypeLinkFilteringData) }];
    }

    void waitUntilDidHandleRequest()
    {
        Util::run(&m_didHandleRequest);
    }

private:
    std::unique_ptr<InstanceMethodSwizzler> m_swizzler;
    bool m_didHandleRequest { false };
};
#endif // HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

static RetainPtr<TestWKWebView> createWebViewWithAdvancedPrivacyProtections(BOOL enabled = YES, RetainPtr<WKWebpagePreferences>&& preferences = { }, WKWebsiteDataStore *dataStore = nil, bool enableResourceLoadStatistics = YES)
{
    auto store = dataStore ?: WKWebsiteDataStore.nonPersistentDataStore;
    store._resourceLoadStatisticsEnabled = enableResourceLoadStatistics;

    auto policies = enabled
        ? _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry | _WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicyRequestValidation | _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters
        : _WKWebsiteNetworkConnectionIntegrityPolicyNone;

    preferences = preferences ?: adoptNS([WKWebpagePreferences new]);
    [preferences _setNetworkConnectionIntegrityPolicy:policies];

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:store];
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    [configuration setDefaultWebpagePreferences:preferences.get()];
    if (!enabled) {
        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"FilterLinkDecorationByDefaultEnabled"]) {
                [[configuration preferences] _setEnabled:YES forFeature:feature];
                break;
            }
        }
    }

    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
}

#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenPrivacyProtectionsAreDisabled)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections(NO);
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, DoNotRemoveTrackingQueryParametersWhenResourceLoadStatisticsAreDisabled)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections(NO, { }, nil, NO);
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenNavigating)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenNavigatingWithContentBlockersDisabled)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };

    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setContentBlockersEnabled:NO];
    auto webView = createWebViewWithAdvancedPrivacyProtections(YES, preferences.get());
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

#if defined(HAS_WEB_PRIVACY_LINK_FILTERING_RULE_PATH)
TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersByDomain)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"a.com", @"", @"" ], @[ @"", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    auto url = [NSURL URLWithString:@"https://b.com/bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://b.com/bundle-file/simple.html?foo=10&garply=20", [webView URL].absoluteString);

    url = [NSURL URLWithString:@"https://a.com/bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://a.com/bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersByPath)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"/abc/", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?foo=10&garply=20", [webView URL].absoluteString);

    url = [NSURL URLWithString:@"https://bundle-file/abc/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/abc/simple.html?garply=20", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersByDomainAndPath)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"a.com", @"", @"" ], @[ @"/abc/", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    auto url = [NSURL URLWithString:@"https://b.com/bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://b.com/bundle-file/simple.html?foo=10&garply=20", [webView URL].absoluteString);

    url = [NSURL URLWithString:@"https://a.com/bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://a.com/bundle-file/simple.html?foo=10&garply=20", [webView URL].absoluteString);

    url = [NSURL URLWithString:@"https://b.com/bundle-file/abc/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://b.com/bundle-file/abc/simple.html?foo=10&garply=20", [webView URL].absoluteString);

    url = [NSURL URLWithString:@"https://a.com/bundle-file/abc/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://a.com/bundle-file/abc/simple.html?garply=20", [webView URL].absoluteString);
}
#endif

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenCopyingURL)
{
#if PLATFORM(MAC)
    auto pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard clearContents];
#else
    auto pasteboard = UIPasteboard.generalPasteboard;
    pasteboard.items = @[ ];
#endif

    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    auto url = [NSURL URLWithString:@"https://bundle-file/clipboard.html"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];

    auto arguments = @{
        @"type" : @"text/plain",
        @"string" : @"http://example.com?foo=09847029&hello=world"
    };

    [webView callAsyncJavaScriptAndWait:@"writeStringToClipboard(type, string)" arguments:arguments];
#if PLATFORM(MAC)
    auto copiedString = [pasteboard stringForType:NSPasteboardTypeString];
#else
    auto copiedString = dynamic_objc_cast<NSString>([pasteboard valueForPasteboardType:UTTypeUTF8PlainText.identifier]);
#endif
    EXPECT_WK_STREQ("http://example.com/?hello=world", copiedString);
}

static void copyURLWithQueryParameters()
{
    auto stringToCopy = @"https://webkit.org/?foo=20&garply=hello";
#if PLATFORM(MAC)
    auto pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard declareTypes:@[NSPasteboardTypeString] owner:nil];
    [pasteboard setString:stringToCopy forType:NSPasteboardTypeURL];
    [pasteboard setString:stringToCopy forType:NSPasteboardTypeString];
#else
    auto pasteboard = UIPasteboard.generalPasteboard;
    [pasteboard setValue:[NSURL URLWithString:stringToCopy] forPasteboardType:UTTypeURL.identifier];
    [pasteboard setValue:stringToCopy forPasteboardType:UTTypeUTF8PlainText.identifier];
#endif
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersFromDataTransferWhenPastingURL)
{
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };
    copyURLWithQueryParameters();

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];
    [webView paste:nil];

    EXPECT_WK_STREQ("https://webkit.org/?garply=hello", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("https://webkit.org/?garply=hello", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenPastingURL)
{
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };
    copyURLWithQueryParameters();

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<body></body><script>document.body.focus()</script>"];
    [webView paste:nil];

    EXPECT_WK_STREQ("https://webkit.org/?garply=hello", [[webView contentsAsString] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]);
    EXPECT_WK_STREQ("https://webkit.org/?garply=hello", [webView stringByEvaluatingJavaScript:@"document.querySelector('a').href"]);
}

TEST(AdvancedPrivacyProtections, UpdateTrackingQueryParametersAfterInitialization)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo" ], @[ @"" ], @[ @"" ] };
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20&bar=30&baz=40", [webView URL].absoluteString);

    swizzler.update(@[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ]);
    swizzler.waitUntilDidHandleRequest();

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenDecidingNavigationPolicy)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView synchronouslyLoadHTMLString:@"<body><a href='https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40'>Link</a></body>"];

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    bool didCallDecisionHandler = false;
    RetainPtr<NSURL> targetURL;
    [navigationDelegate setDecidePolicyForNavigationAction:[&](WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        targetURL = action.request.URL;
        didCallDecisionHandler = true;
        decisionHandler(WKNavigationActionPolicyCancel);
    }];

    [webView objectByEvaluatingJavaScript:@"document.querySelector('a').click()"];
    Util::run(&didCallDecisionHandler);

    EXPECT_WK_STREQ([targetURL absoluteString], "https://bundle-file/simple.html?garply=20");
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersForMainResourcesOnly)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler blockListSwizzler { @[ @"blockThis" ], @[ @"" ], @[ @"" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    __block Vector<std::pair<String, String>> adjustedURLStrings;
    [navigationDelegate setDidChangeLookalikeCharactersFromURL:^(WKWebView *, NSURL *originalURL, NSURL *adjustedURL) {
        adjustedURLStrings.append({ { originalURL.absoluteString }, { adjustedURL.absoluteString } });
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];

    RetainPtr url = [NSURL URLWithString:@"https://bundle-file/simple.html?blockThis=yes"];
    [webView loadRequest:[NSURLRequest requestWithURL:url.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    auto appendAndLoadElement = [&](NSString *tagName, NSString *source) {
        [webView callAsyncJavaScriptAndWait:[NSString stringWithFormat:@"return new Promise(resolve => {"
            "  const element = document.createElement('%@');"
            "  element.addEventListener('load', () => resolve(true));"
            "  element.addEventListener('error', () => resolve(false));"
            "  element.src = '%@';"
            "  document.body.appendChild(element);"
            "});", tagName, source]];
    };

    appendAndLoadElement(@"img", @"https://bundle-file/icon.png?blockThis=notThisOne");
    appendAndLoadElement(@"script", @"https://bundle-file/deferred-script.js?blockThis=notThisEither");
    appendAndLoadElement(@"iframe", @"https://bundle-file/simple2.html?blockThis=yesThisToo");

    EXPECT_EQ(adjustedURLStrings.size(), 2U);
    {
        auto& [original, adjusted] = adjustedURLStrings[0];
        EXPECT_WK_STREQ([url absoluteString], original);
        EXPECT_WK_STREQ("https://bundle-file/simple.html", adjusted);
    }
    {
        auto& [original, adjusted] = adjustedURLStrings[1];
        EXPECT_WK_STREQ("https://bundle-file/simple2.html?blockThis=yesThisToo", original);
        EXPECT_WK_STREQ("https://bundle-file/simple2.html", adjusted);
    }
}

TEST(AdvancedPrivacyProtections, ApplyNavigationalProtectionsAfterMultiplePSON)
{
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ], @[ @"", @"", @"" ], @[ @"", @"", @"" ] };
    HTTPServer server({
        { "/landing"_s, { "<script>window.result = document.referrer;</script>"_s } },
        { "/landing?garply=20"_s, { } }
    }, HTTPServer::Protocol::Http);

    auto refreshHeaderContent = makeString("1;URL=http://127.0.0.1:"_s, server.port(), "/landing?foo=10&garply=20&bar=30&baz=40"_s);
    server.addResponse("/"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Refresh"_s, refreshHeaderContent } }, "body"_s });

    auto webView = createWebViewWithAdvancedPrivacyProtections(NO);
    __block bool didCallDecisionHandler { false };
    __block bool finishedSuccessfully { false };
    __block RetainPtr<NSURL> targetURL;

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([action.request.URL.host isEqualToString:@"127.0.0.1"])
            didCallDecisionHandler = true;
        targetURL = action.request.URL;
        preferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:makeString("http://localhost:"_s, server.port(), "/"_s)]]];
    Util::run(&didCallDecisionHandler);
    finishedSuccessfully = false;
    Util::run(&finishedSuccessfully);

    NSString *result = [webView objectByEvaluatingJavaScript:@"location.href"];
    EXPECT_WK_STREQ(result, makeString("http://127.0.0.1:"_s, server.port(), "/landing?garply=20"_s));

    result = [webView objectByEvaluatingJavaScript:@"window.result"];
    EXPECT_WK_STREQ(@"", result);
}

static RetainPtr<TestWKWebView> setUpWebViewForTestingQueryParameterHiding(NSString *pageSource, NSString *requestURLString, NSString *referrer = @"https://webkit.org")
{
    auto *store = WKWebsiteDataStore.nonPersistentDataStore;
    store._resourceLoadStatisticsEnabled = YES;

    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnabled];

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:store];
    [configuration setDefaultWebpagePreferences:preferences.get()];

    auto handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *path = task.request.URL.path;
        NSString *type = nil;
        NSString *result = nil;
        NSString *host = task.request.URL.host;
        if ([path containsString:@"index.html"]) {
            result = pageSource;
            type = @"text/html";
        } else if ([path containsString:@"script-noop.js"]) {
            result = @"while (false);";
            type = @"text/javascript";
        } else if ([path containsString:@"script.js"]) {
            result = [NSString stringWithFormat:@"results.push(`%@: ${document.URL}`)", host];
            type = @"text/javascript";
        } else if ([path containsString:@"script-async.js"]) {
            result = [NSString stringWithFormat:@"setTimeout(() => results.push(`%@: ${document.URL}`), 0)", host];
            type = @"text/javascript";
        }

        if (!result) {
            [task didFailWithError:[NSError errorWithDomain:@"TestWebKitAPI" code:1 userInfo:nil]];
            return;
        }

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:type expectedContentLength:[result length] textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[result dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];

    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"custom"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    auto request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:requestURLString]];
    [request setValue:referrer forHTTPHeaderField:@"referer"];
    [webView synchronouslyLoadRequest:request];
    [webView callAsyncJavaScriptAndWait:@"return new Promise(resolve => setTimeout(resolve, 0))"];

    return webView;
}

TEST(AdvancedPrivacyProtections, HideQueryParametersAfterCrossSiteNavigation)
{
    AllowedLinkFilteringDataRequestSwizzler allowListSwizzler { @[ @"preserveMe" ] };
    QueryParameterRequestSwizzler blockListSwizzler { @[ @"blockMe" ], @[ @"" ], @[ @"" ] };

    auto webView = setUpWebViewForTestingQueryParameterHiding(@"<!DOCTYPE html>"
        "<html>"
        "    <head>"
        "        <script src='custom://thirdparty/script-noop.js'></script>"
        "        <script>"
        "        window.results = [`firstparty: ${document.URL}`]"
        "        </script>"
        "        <script src='custom://firstparty/script.js'></script>"
        "        <script src='custom://thirdparty/script.js'></script>"
        "        <script src='custom://firstparty/script-async.js'></script>"
        "        <script src='custom://thirdparty/script-async.js'></script>"
        "    </head>"
        "    <body>Hello world</body>"
        "</html>", @"custom://firstparty/index.html?hello=123&world=456");

    NSArray<NSString *> *results = [webView objectByEvaluatingJavaScript:@"window.results"];

    constexpr auto* expectedFirstPartyResult = "firstparty: custom://firstparty/index.html?hello=123&world=456";
    constexpr auto* expectedThirdPartyResult = "thirdparty: custom://firstparty/index.html";

    EXPECT_EQ(5U, results.count);
    EXPECT_WK_STREQ(expectedFirstPartyResult, results[0]);
    EXPECT_WK_STREQ(expectedFirstPartyResult, results[1]);
    EXPECT_WK_STREQ(expectedThirdPartyResult, results[2]);
    EXPECT_WK_STREQ(expectedFirstPartyResult, results[3]);
    EXPECT_WK_STREQ(expectedThirdPartyResult, results[4]);
}

TEST(AdvancedPrivacyProtections, DoNotHideQueryParametersForFirstPartyScript)
{
    AllowedLinkFilteringDataRequestSwizzler allowListSwizzler { @[ @"preserveMe" ] };
    QueryParameterRequestSwizzler blockListSwizzler { @[ @"blockMe" ], @[ @"" ], @[ @"" ] };

    auto webView = setUpWebViewForTestingQueryParameterHiding(@"<!DOCTYPE html>"
        "<html>"
        "    <head>"
        "        <script>"
        "        window.results = [];"
        "        </script>"
        "        <script src='custom://thirdparty/script-noop.js'></script>"
        "        <script src='custom://firstparty.com/script.js'></script>"
        "        <script src='custom://firstparty.co.jp/script.js'></script>"
        "        <script src='custom://firstparty.co/script.js'></script>"
        "        <script src='custom://hello.firstparty.net/script.js'></script>"
        "        <script src='custom://notfirstparty.co.uk/script.js'></script>"
        "        <script src='custom://firstparty.nope.co.uk/script.js'></script>"
        "    </head>"
        "    <body>Hello world</body>"
        "</html>", @"custom://firstparty.co.uk/index.html?hello=123");

    NSArray<NSString *> *results = [webView objectByEvaluatingJavaScript:@"window.results"];

    EXPECT_EQ(6U, results.count);
    EXPECT_WK_STREQ("firstparty.com: custom://firstparty.co.uk/index.html?hello=123", results[0]);
    EXPECT_WK_STREQ("firstparty.co.jp: custom://firstparty.co.uk/index.html?hello=123", results[1]);
    EXPECT_WK_STREQ("firstparty.co: custom://firstparty.co.uk/index.html?hello=123", results[2]);
    EXPECT_WK_STREQ("hello.firstparty.net: custom://firstparty.co.uk/index.html?hello=123", results[3]);
    EXPECT_WK_STREQ("notfirstparty.co.uk: custom://firstparty.co.uk/index.html", results[4]);
    EXPECT_WK_STREQ("firstparty.nope.co.uk: custom://firstparty.co.uk/index.html", results[5]);
}

TEST(AdvancedPrivacyProtections, DoNotHideQueryParametersAfterEffectiveSameSiteNavigation)
{
    AllowedLinkFilteringDataRequestSwizzler allowListSwizzler { @[ @"preserveMe" ] };

    auto webView = setUpWebViewForTestingQueryParameterHiding(@"<!DOCTYPE html>"
        "<html>"
        "    <head>"
        "        <script>"
        "        window.results = [];"
        "        </script>"
        "        <script src='custom://thirdparty/script-noop.js'></script>"
        "        <script src='custom://thirdparty/script.js'></script>"
        "    </head>"
        "    <body>Hello world</body>"
        "</html>", @"custom://firstparty.co.uk/index.html?hello=123", @"custom://firstparty.com");

    NSArray<NSString *> *results = [webView objectByEvaluatingJavaScript:@"window.results"];

    EXPECT_EQ(1U, results.count);
    EXPECT_WK_STREQ("thirdparty: custom://firstparty.co.uk/index.html?hello=123", results[0]);
}

TEST(AdvancedPrivacyProtections, LinkPreconnectUsesEnhancedPrivacy)
{
    auto createMarkupString = [](unsigned serverPort) {
        return makeString("<!DOCTYPE html>"
            "<html>"
            "    <head>"
            "        <link rel='preconnect' href='http://127.0.0.1:"_s, serverPort, "/'>"
            "    </head>"
            "    <body>Hello world</body>"
            "</html>"_s);
    };

    HTTPServer server { { }, HTTPServer::Protocol::Http };
    server.addResponse("/index.html"_s, { createMarkupString(server.port()) });

    auto webView = createWebViewWithAdvancedPrivacyProtections(YES, nil, WKWebsiteDataStore.defaultDataStore);
    auto request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://localhost:%u/index.html", server.port()]]];
    request._useEnhancedPrivacyMode = YES;
    [webView synchronouslyLoadRequest:request];

    do {
        Util::runFor(10_ms);
    } while (server.totalConnections() < 2);

#if PLATFORM(MAC)
    auto logMessages = [webView collectLogsForNewConnections];
    if ([logMessages.firstObject containsString:@"CFNetwork"]) {
        // The old HTTP stack in CFNetwork creates one connection per TCP/QUIC connection, but the new HTTP stack creates one connection per task.
        // This path can be removed when this test stops running on macOS 14 / iOS 17 or below.
        EXPECT_EQ([logMessages count], 2U);
    } else
        EXPECT_EQ([logMessages count], 4U);
    for (NSString *message : logMessages)
        EXPECT_TRUE([message containsString:@"enhanced privacy"]);
#endif
}

#endif // HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

static RetainPtr<TestWKWebView> webViewAfterCrossSiteNavigationWithReducedPrivacy(NSString *initialURLString, bool withRedirect = false)
{
    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyNone];

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setDefaultWebpagePreferences:preferences.get()];

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:initialURLString]]];
    [navigationDelegate waitForDidFinishNavigation];

    [navigationDelegate setDecidePolicyForNavigationActionWithPreferences:[&](WKNavigationAction *action, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters | _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    }];

    [webView evaluateJavaScript:@"document.querySelector('a').click()" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    if (withRedirect)
        [navigationDelegate waitForDidFinishNavigation];

    return webView;
}

TEST(AdvancedPrivacyProtections, DoNotHideReferrerAfterReducingPrivacyProtections)
{
    HTTPServer server({
        { "/index2.html"_s, { "<script>window.result = document.referrer;</script>"_s } },
    }, HTTPServer::Protocol::Http);

    server.addResponse("/index1.html"_s, { makeString("<a href='http://127.0.0.1:"_s, server.port(), "/index2.html'>Link</a>"_s) });

    auto webView = webViewAfterCrossSiteNavigationWithReducedPrivacy(makeString("http://localhost:"_s, server.port(), "/index1.html"_s));

    NSString *result = [webView objectByEvaluatingJavaScript:@"window.result"];
    NSString *expectedReferrer = [NSString stringWithFormat:@"http://localhost:%d/", server.port()];
    EXPECT_WK_STREQ(expectedReferrer, result);
}

TEST(AdvancedPrivacyProtections, DoNotHideReferrerAfterReducingPrivacyProtectionsWithJSRedirect)
{
    HTTPServer server({
        { "/destination.html"_s, { "<script>window.result = document.referrer;</script>"_s } },
    }, HTTPServer::Protocol::Http);

    server.addResponse("/source.html"_s, { makeString("<a href='http://127.0.0.1:"_s, server.port(), "/redirect.html'>Link</a>"_s) });
    server.addResponse("/redirect.html"_s, { makeString("<script>window.location = 'http://localhost:"_s, server.port(), "/destination.html';</script>"_s) });

    auto webView = webViewAfterCrossSiteNavigationWithReducedPrivacy(makeString("http://localhost:"_s, server.port(), "/source.html"_s), true);

    NSString *result = [webView objectByEvaluatingJavaScript:@"window.result"];
    NSString *expectedReferrer = [NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()];
    EXPECT_WK_STREQ(expectedReferrer, result);
}

TEST(AdvancedPrivacyProtections, DoNotHideReferrerAfterReducingPrivacyProtectionsWithHTTPRedirect)
{
    HTTPServer server({
        { "/destination.html"_s, { "<script>window.result = document.referrer;</script>"_s } },
    }, HTTPServer::Protocol::Http);

    server.addResponse("/source.html"_s, { makeString("<a href='http://127.0.0.1:"_s, server.port(), "/redirect'>Link</a>"_s) });
    server.addResponse("/redirect"_s, { 302, {{"Location"_s, makeString("http://localhost:"_s, server.port(), "/destination.html"_s) }}, "redirecting..."_s });

    auto webView = webViewAfterCrossSiteNavigationWithReducedPrivacy(makeString("http://localhost:"_s, server.port(), "/source.html"_s));

    NSString *result = [webView objectByEvaluatingJavaScript:@"window.result"];
    NSString *expectedReferrer = [NSString stringWithFormat:@"http://localhost:%d/", server.port()];
    EXPECT_WK_STREQ(expectedReferrer, result);
}

TEST(AdvancedPrivacyProtections, HideScreenMetricsFromBindings)
{
#if PLATFORM(IOS_FAMILY)
    IPadUserInterfaceSwizzler userInterfaceSwizzler;
#endif

    [TestProtocol registerWithScheme:@"https"];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
#if PLATFORM(MAC)
    [uiDelegate setGetWindowFrameWithCompletionHandler:^(WKWebView *view, void(^completionHandler)(CGRect)) {
        auto viewBounds = view.bounds;
        return completionHandler(CGRectMake(100, 100, CGRectGetWidth(viewBounds), CGRectGetHeight(viewBounds)));
    }];
#endif

    auto webView = createWebViewWithAdvancedPrivacyProtections();

    [webView setUIDelegate:uiDelegate.get()];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://bundle-file/simple-responsive-page.html"]]];

    auto bruteForceMediaQueryScript = [](NSString *key) -> NSString * {
        return [NSString stringWithFormat:@"(function() {"
            "    const maximumValue = 10000;"
            "    for (let i = 0; i < maximumValue; ++i) {"
            "        if (matchMedia(`(%@ <= ${i}px)`).matches)"
            "            return i;"
            "    }"
            "    return maximumValue;"
            "})()", key];
    };

    auto innerWidth = [[webView objectByEvaluatingJavaScript:@"innerWidth"] intValue];
    auto innerHeight = [[webView objectByEvaluatingJavaScript:@"innerHeight"] intValue];

    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenX"] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenY"] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availLeft"] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availTop"] intValue]);
    EXPECT_EQ(innerWidth, [[webView objectByEvaluatingJavaScript:@"screen.availWidth"] intValue]);
    EXPECT_EQ(innerHeight, [[webView objectByEvaluatingJavaScript:@"screen.availHeight"] intValue]);
    EXPECT_EQ(innerWidth, [[webView objectByEvaluatingJavaScript:@"outerWidth"] intValue]);
    EXPECT_EQ(innerHeight, [[webView objectByEvaluatingJavaScript:@"outerHeight"] intValue]);
    EXPECT_EQ(innerWidth, [[webView objectByEvaluatingJavaScript:bruteForceMediaQueryScript(@"device-width")] intValue]);
    EXPECT_EQ(innerHeight, [[webView objectByEvaluatingJavaScript:bruteForceMediaQueryScript(@"device-height")] intValue]);
}

TEST(AdvancedPrivacyProtections, HideScreenSizeWithScaledPage)
{
#if PLATFORM(IOS_FAMILY)
    IPadUserInterfaceSwizzler userInterfaceSwizzler;
#endif

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    auto screenSizeBeforeScaling = [webView computeScreenSizeForBindings];
    EXPECT_EQ(800, screenSizeBeforeScaling.width);
    EXPECT_EQ(600, screenSizeBeforeScaling.height);

    [webView _setPageScale:2 withOrigin:CGPointZero];
    [webView waitForNextPresentationUpdate];

    auto screenSizeAfterScaling = [webView computeScreenSizeForBindings];
    EXPECT_EQ(800, screenSizeAfterScaling.width);
    EXPECT_EQ(600, screenSizeAfterScaling.height);
}

#if PLATFORM(IOS_FAMILY)

static std::unique_ptr<InstanceMethodSwizzler> makeScreenSizeSwizzler(CGSize size)
{
    return makeUnique<InstanceMethodSwizzler>(
        UIScreen.class,
        @selector(_referenceBounds),
        imp_implementationWithBlock([size](UIScreen *) {
            return CGRectMake(0, 0, size.width, size.height);
        })
    );
}

TEST(AdvancedPrivacyProtections, ClampScreenSizeToFixedValues)
{
    IPhoneUserInterfaceSwizzler userInterfaceSwizzler;
    auto runTestWithScreenSize = [](CGSize actualScreenSize, CGSize expectedSize) {
        auto screenSizeSwizzler = makeScreenSizeSwizzler(actualScreenSize);
        auto webView = createWebViewWithAdvancedPrivacyProtections();
        [webView synchronouslyLoadTestPageNamed:@"simple"];

        auto sizeForBindings = [webView computeScreenSizeForBindings];
        EXPECT_EQ(expectedSize.width, sizeForBindings.width);
        EXPECT_EQ(expectedSize.height, sizeForBindings.height);
    };

    runTestWithScreenSize(CGSizeMake(320, 568), CGSizeMake(320, 568));
    runTestWithScreenSize(CGSizeMake(388, 844), CGSizeMake(390, 844));
    runTestWithScreenSize(CGSizeMake(390, 848), CGSizeMake(390, 844));
    runTestWithScreenSize(CGSizeMake(440, 900), CGSizeMake(414, 896));
}

#endif // PLATFORM(IOS_FAMILY)

TEST(AdvancedPrivacyProtections, AddNoiseToWebAudioAPIs)
{
    auto testURL = [NSBundle.test_resourcesBundle URLForResource:@"audio-fingerprinting" withExtension:@"html"];
    auto resourcesURL = NSBundle.test_resourcesBundle.resourceURL;

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webView _test_waitForDidFinishNavigation];

    auto checkFingerprintForNoise = [&](NSString *functionName) {
        auto scriptToRun = [NSString stringWithFormat:@"return %@()", functionName];
        auto values = std::pair {
            [[webView callAsyncJavaScriptAndWait:scriptToRun] floatValue],
            [[webView callAsyncJavaScriptAndWait:scriptToRun] floatValue]
        };
        EXPECT_NE(values.first, values.second);
    };

    checkFingerprintForNoise(@"testOscillatorCompressor");
    checkFingerprintForNoise(@"testOscillatorCompressorWorklet");
    checkFingerprintForNoise(@"testOscillatorCompressorAnalyzer");
    checkFingerprintForNoise(@"testLoopingOscillatorCompressorBiquadFilter");
}

TEST(AdvancedPrivacyProtections, AddNoiseToWebAudioAPIsAfterMultiplePSON)
{
    [TestProtocol registerWithScheme:@"https"];

    HTTPServer server({
        { "/landing"_s, { "<script>window.result = document.referrer;</script>"_s } },
    }, HTTPServer::Protocol::Http);

    server.addResponse("/"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Refresh"_s, "1;URL=https://bundle-file/audio-fingerprinting.html"_s } }, "body"_s });

    auto webView = createWebViewWithAdvancedPrivacyProtections(NO);
    __block bool didCallDecisionHandler { false };
    __block bool finishedSuccessfully { false };

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([action.request.URL.host isEqualToString:@"bundle-file"])
            didCallDecisionHandler = true;
        preferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry | _WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicyRequestValidation | _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:makeString("http://localhost:"_s, server.port(), "/"_s)]]];
    Util::run(&didCallDecisionHandler);
    finishedSuccessfully = false;
    Util::run(&finishedSuccessfully);

    auto checkFingerprintForNoise = [&](NSString *functionName) {
        auto scriptToRun = [NSString stringWithFormat:@"return %@()", functionName];
        auto values = std::pair {
            [[webView callAsyncJavaScriptAndWait:scriptToRun] floatValue],
            [[webView callAsyncJavaScriptAndWait:scriptToRun] floatValue]
        };
        EXPECT_NE(values.first, values.second);
    };

    checkFingerprintForNoise(@"testOscillatorCompressor");
    checkFingerprintForNoise(@"testOscillatorCompressorWorklet");
    checkFingerprintForNoise(@"testOscillatorCompressorAnalyzer");
    checkFingerprintForNoise(@"testLoopingOscillatorCompressorBiquadFilter");
}

TEST(AdvancedPrivacyProtections, AddNoiseToWebAudioAPIsAfterReducingPrivacyProtectionsAndMultiplePSON)
{
    [TestProtocol registerWithScheme:@"https"];

    HTTPServer server({
        { "/index2.html"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s }, { "Refresh"_s, "1;URL=https://bundle-file/audio-fingerprinting.html"_s } }, "body"_s } },
    }, HTTPServer::Protocol::Http);

    server.addResponse("/index1.html"_s, { makeString("<a href='http://127.0.0.1:"_s, server.port(), "/index2.html'>Link</a>"_s) });

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDecidePolicyForNavigationActionWithPreferences:[&](WKNavigationAction *action, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters | _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    }];

    auto webView = webViewAfterCrossSiteNavigationWithReducedPrivacy(makeString("http://localhost:"_s, server.port(), "/index1.html"_s));

    [webView setNavigationDelegate:navigationDelegate.get()];
    [navigationDelegate waitForDidFinishNavigation];

    auto checkFingerprintForNoise = [&](NSString *functionName) {
        auto scriptToRun = [NSString stringWithFormat:@"return %@()", functionName];
        auto values = std::pair {
            [[webView callAsyncJavaScriptAndWait:scriptToRun] floatValue],
            [[webView callAsyncJavaScriptAndWait:scriptToRun] floatValue]
        };
        EXPECT_NE(values.first, values.second);
    };

    checkFingerprintForNoise(@"testOscillatorCompressor");
}

TEST(AdvancedPrivacyProtections, VerifyHashFromNoisyCanvas2DAPI)
{
    auto testURL = [NSBundle.test_resourcesBundle URLForResource:@"canvas-fingerprinting" withExtension:@"html"];
    auto resourcesURL = NSBundle.test_resourcesBundle.resourceURL;

    auto webView1 = createWebViewWithAdvancedPrivacyProtections(NO);
    auto webView2 = createWebViewWithAdvancedPrivacyProtections(NO);
    auto webViewWithPrivacyProtections1 = createWebViewWithAdvancedPrivacyProtections();
    auto webViewWithPrivacyProtections2 = createWebViewWithAdvancedPrivacyProtections();

    [webView1 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webView1 _test_waitForDidFinishNavigation];
    [webView2 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webView2 _test_waitForDidFinishNavigation];
    [webViewWithPrivacyProtections1 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webViewWithPrivacyProtections1 _test_waitForDidFinishNavigation];
    [webViewWithPrivacyProtections2 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webViewWithPrivacyProtections2 _test_waitForDidFinishNavigation];

    auto scriptToRun = @"return fullCanvasHash()";
    auto values = std::pair {
        [webView1 callAsyncJavaScriptAndWait:scriptToRun],
        [webView2 callAsyncJavaScriptAndWait:scriptToRun]
    };
#if !CPU(X86_64)
    // FIXME: Enable this check on x86_64 when rdar://115137641 is resolved.
    EXPECT_TRUE([values.first isEqualToString:values.second]);
#endif

    values = std::pair {
        [webView1 callAsyncJavaScriptAndWait:scriptToRun],
        [webViewWithPrivacyProtections1 callAsyncJavaScriptAndWait:scriptToRun]
    };
    EXPECT_FALSE([values.first isEqualToString:values.second]);

    values = std::pair {
        [webViewWithPrivacyProtections1 callAsyncJavaScriptAndWait:scriptToRun],
        [webViewWithPrivacyProtections2 callAsyncJavaScriptAndWait:scriptToRun],
    };
    EXPECT_FALSE([values.first isEqualToString:values.second]);
}

TEST(AdvancedPrivacyProtections, VerifyPixelsFromNoisyCanvas2DAPI)
{
    constexpr auto zeroPrefix = 380;
    constexpr auto channelsPerPixel = 4;

    auto testURL = [NSBundle.test_resourcesBundle URLForResource:@"canvas-fingerprinting" withExtension:@"html"];
    auto resourcesURL = NSBundle.test_resourcesBundle.resourceURL;

    auto webView1 = createWebViewWithAdvancedPrivacyProtections(NO);
    auto webView2 = createWebViewWithAdvancedPrivacyProtections(NO);
    auto webViewWithPrivacyProtections1 = createWebViewWithAdvancedPrivacyProtections();
    auto webViewWithPrivacyProtections2 = createWebViewWithAdvancedPrivacyProtections();

    [webView1 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webView1 _test_waitForDidFinishNavigation];

    [webView2 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webView2 _test_waitForDidFinishNavigation];

    [webViewWithPrivacyProtections1 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webViewWithPrivacyProtections1 _test_waitForDidFinishNavigation];

    [webViewWithPrivacyProtections2 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webViewWithPrivacyProtections2 _test_waitForDidFinishNavigation];

    auto checkCanvasPixels = [&](NSString *functionName, size_t length, Function<void(NSDictionary *, NSDictionary *, int, int)> expect) {
        // This is a larger tolerance than we'd like, but the lossy nature of the premultiplied conversion doesn't give us many options.
        constexpr auto maxPixelDifferenceTolerance = 8;
        auto scriptToRun = [NSString stringWithFormat:@"return %@(%zu)", functionName, length];

        NSDictionary *arr1 = [webView1 callAsyncJavaScriptAndWait:scriptToRun];
        NSDictionary *arr2 = [webView2 callAsyncJavaScriptAndWait:scriptToRun];

        EXPECT_TRUE([arr1 isEqualToDictionary:arr2]);
        EXPECT_NE(arr1.count, 0u);
        EXPECT_NE(arr2.count, 0u);
        EXPECT_EQ(arr1.count, arr2.count);

        NSDictionary *arrWithPrivacyProtections1 = [webViewWithPrivacyProtections1 callAsyncJavaScriptAndWait:scriptToRun];

        expect(arr1, arrWithPrivacyProtections1, maxPixelDifferenceTolerance, __LINE__);
        EXPECT_NE(arrWithPrivacyProtections1.count, 0u);

        NSDictionary *arrWithPrivacyProtections2 = [webViewWithPrivacyProtections2 callAsyncJavaScriptAndWait:scriptToRun];

        expect(arr1, arrWithPrivacyProtections2, maxPixelDifferenceTolerance, __LINE__);
        EXPECT_NE(arrWithPrivacyProtections2.count, 0u);

        expect(arrWithPrivacyProtections1, arrWithPrivacyProtections2, maxPixelDifferenceTolerance * 2, __LINE__);
    };

    auto checkCanvasPixelsEqual = [&](NSString *functionName, int length, int primaryLine) {
        checkCanvasPixels(functionName, length, [primaryLine](NSDictionary *arr1, NSDictionary *arr2, int, int secondaryLine) {
            EXPECT_TRUE([arr1 isEqualToDictionary:arr2]) << "Test at line " << primaryLine << " failed at line " << secondaryLine << "\narr1: " << arr1.description << "\narr2: " << arr2.description;
        });
    };

    auto checkCanvasPixelsNotEqual = [&](NSString *functionName, int length, int primaryLine) {
        checkCanvasPixels(functionName, length, [primaryLine](NSDictionary *arr1, NSDictionary *arr2, int tolerance, int secondaryLine) {
            EXPECT_FALSE([arr1 isEqualToDictionary:arr2]) << "Test at line " << primaryLine << " failed at line " << secondaryLine << "\narr1: " << arr1.description << "\narr2: " << arr2.description;
            for (auto i = 0u; i < arr1.count; i += 4) {
                auto* idx = [NSString stringWithFormat:@"%d", i];
                auto* idxPlus1 = [NSString stringWithFormat:@"%d", i + 1];
                auto* idxPlus2 = [NSString stringWithFormat:@"%d", i + 2];
                auto* idxPlus3 = [NSString stringWithFormat:@"%d", i + 3];
                int alpha1 = [[arr1 valueForKey:idxPlus3] intValue];
                int alpha2 = [[arr2 valueForKey:idxPlus3] intValue];
                EXPECT_LE(std::abs(alpha1 - alpha2), (tolerance / 2)) << "Test at line " << primaryLine << " failed at line " << secondaryLine << "\nAlpha at index (+3): " << i << " is not within tolerance: " << (tolerance / 2) << ". arr1 value: " << alpha1 << ", arr2 value: " << alpha2;

                auto red1 = [[arr1 valueForKey:idx] intValue];
                auto red2 = [[arr2 valueForKey:idx] intValue];
                EXPECT_LE(std::abs(std::round(static_cast<float>(red1 * alpha1) / 255) - std::round(static_cast<float>(red2 * alpha2) / 255)), tolerance) << "Test at line " << primaryLine << " failed at line " << secondaryLine << "\nindex: " << i << " is not within tolerance: " << tolerance << ". arr1 value: " << red1 << " (alpha: " << alpha1 << "), arr2 value: " << red2 << " (alpha: " << alpha2 << ")";

                auto green1 = [[arr1 valueForKey:idxPlus1] intValue];
                auto green2 = [[arr2 valueForKey:idxPlus1] intValue];
                EXPECT_LE(std::abs(std::round(static_cast<float>(green1 * alpha1) / 255) - std::round(static_cast<float>(green2 * alpha2) / 255)), tolerance) << "Test at line " << primaryLine << " failed at line " << secondaryLine << "\nindex (+1): " << i << " is not within tolerance: " << tolerance << ". arr1 value: " << green1 << " (alpha: " << alpha1 << "), arr2 value: " << green2 << " (alpha: " << alpha2 << ")";

                auto blue1 = [[arr1 valueForKey:idxPlus2] intValue];
                auto blue2 = [[arr2 valueForKey:idxPlus2] intValue];
                EXPECT_LE(std::abs(std::round(static_cast<float>(blue1 * alpha1) / 255) - std::round(static_cast<float>(blue2 * alpha2) / 255)), tolerance) << "Test at line " << primaryLine << " failed at line " << secondaryLine << "\nindex (+2): " << i << " is not within tolerance: " << tolerance << ". arr1 value: " << blue1 << " (alpha: " << alpha1 << "), arr2 value: " << blue2 << " (alpha: " << alpha2 << ")";
            }
        });
    };

    checkCanvasPixelsEqual(@"initialTextCanvasImageDataAsObject", zeroPrefix * channelsPerPixel, __LINE__);
    checkCanvasPixelsNotEqual(@"initialTextCanvasImageDataAsObject", 300 * 200 * channelsPerPixel, __LINE__);

    checkCanvasPixelsEqual(@"initialHorizontalLinearGradientCanvasImageDataAsObject", 1 * channelsPerPixel, __LINE__);
    checkCanvasPixelsNotEqual(@"initialHorizontalLinearGradientCanvasImageDataAsObject", 30 * 2 * channelsPerPixel, __LINE__);

    auto scriptToRun = @"return isHorizontalLinearGradientCanvasGradient()";
    EXPECT_TRUE([webView1 callAsyncJavaScriptAndWait:scriptToRun]);
    EXPECT_TRUE([webView2 callAsyncJavaScriptAndWait:scriptToRun]);
    EXPECT_TRUE([webViewWithPrivacyProtections1 callAsyncJavaScriptAndWait:scriptToRun]);
    EXPECT_TRUE([webViewWithPrivacyProtections2 callAsyncJavaScriptAndWait:scriptToRun]);

    checkCanvasPixelsEqual(@"initialVerticalLinearGradientCanvasImageDataAsObject", 2, __LINE__);
    checkCanvasPixelsNotEqual(@"initialVerticalLinearGradientCanvasImageDataAsObject", 2 * 30 * channelsPerPixel, __LINE__);

    scriptToRun = @"return isVerticalLinearGradientCanvasGradient()";
    EXPECT_TRUE([webView1 callAsyncJavaScriptAndWait:scriptToRun]);
    EXPECT_TRUE([webView2 callAsyncJavaScriptAndWait:scriptToRun]);
    EXPECT_TRUE([webViewWithPrivacyProtections1 callAsyncJavaScriptAndWait:scriptToRun]);
    EXPECT_TRUE([webViewWithPrivacyProtections2 callAsyncJavaScriptAndWait:scriptToRun]);

    checkCanvasPixelsEqual(@"initialRadialGradientCanvasImageDataAsObject", 1 * channelsPerPixel, __LINE__);
    checkCanvasPixelsNotEqual(@"initialRadialGradientCanvasImageDataAsObject", 300 * channelsPerPixel, __LINE__);
}

TEST(AdvancedPrivacyProtections, VerifyDataURLFromNoisyWebGLAPI)
{
    auto testURL = [NSBundle.test_resourcesBundle URLForResource:@"canvas-fingerprinting" withExtension:@"html"];
    auto resourcesURL = NSBundle.test_resourcesBundle.resourceURL;

    auto webView1 = createWebViewWithAdvancedPrivacyProtections(NO);
    auto webView2 = createWebViewWithAdvancedPrivacyProtections(NO);
    auto webViewWithPrivacyProtections1 = createWebViewWithAdvancedPrivacyProtections();
    auto webViewWithPrivacyProtections2 = createWebViewWithAdvancedPrivacyProtections();

    [webView1 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webView1 _test_waitForDidFinishNavigation];
    [webView2 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webView2 _test_waitForDidFinishNavigation];
    [webViewWithPrivacyProtections1 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webViewWithPrivacyProtections1 _test_waitForDidFinishNavigation];
    [webViewWithPrivacyProtections2 loadFileRequest:[NSURLRequest requestWithURL:testURL] allowingReadAccessToURL:resourcesURL];
    [webViewWithPrivacyProtections2 _test_waitForDidFinishNavigation];

    auto scriptToRun = @"return webGLGradientCanvasDataURL()";
    auto values = std::pair {
        [webView1 callAsyncJavaScriptAndWait:scriptToRun],
        [webView2 callAsyncJavaScriptAndWait:scriptToRun]
    };
    EXPECT_TRUE([values.first isEqualToString:values.second]);

    values = std::pair {
        [webView1 callAsyncJavaScriptAndWait:scriptToRun],
        [webViewWithPrivacyProtections1 callAsyncJavaScriptAndWait:scriptToRun]
    };
    EXPECT_FALSE([values.first isEqualToString:values.second]);

    values = std::pair {
        [webViewWithPrivacyProtections1 callAsyncJavaScriptAndWait:scriptToRun],
        [webViewWithPrivacyProtections2 callAsyncJavaScriptAndWait:scriptToRun],
    };
    EXPECT_FALSE([values.first isEqualToString:values.second]);
}

TEST(AdvancedPrivacyProtections, Canvas2DQuirks)
{
    using namespace TestWebKitAPI;
    auto *script = @"let canvas = document.createElement(\"canvas\"); canvas.width = 280; canvas.height = 60; let ctx = canvas.getContext(\"2d\"); ctx.fillText(\"<@nv45. F1n63r,Pr1n71n6!\", 10, 40); canvas.toDataURL();";
    auto *paddedScript = [script stringByPaddingToLength:212053 withString:@" " startingAtIndex:0];
    EXPECT_NOT_NULL(paddedScript);
    EXPECT_EQ(paddedScript.length, 212053u);

    const auto dataURL = @"data:image/png;base64,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A";

    HTTPServer httpsServer({
        { "/"_s, { { }, "index"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto webView = createWebViewWithAdvancedPrivacyProtections(YES, { }, dataStore.get());

    __block bool finishedSuccessfully { false };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        completionHandler(WKNavigationActionPolicyAllow);
    };
    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(0, error.code);
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/"]]];

    while (!finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    [webView waitForNextPresentationUpdate];

    __block bool doneEvaluatingJavaScript { false };
    const auto completionHandler = ^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_WK_STREQ(@"", [error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey]);
        EXPECT_WK_STREQ(@"", error.domain);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(dataURL, (NSString *)value);
        doneEvaluatingJavaScript = true;
    };

    [webView evaluateJavaScript:paddedScript completionHandler:completionHandler];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_TRUE(finishedSuccessfully);

    finishedSuccessfully = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/"]]];

    while (!finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    [webView waitForNextPresentationUpdate];

    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:script completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_FALSE([dataURL isEqualToString:(NSString *)value]);
        EXPECT_GT(((NSString *)value).length, 0u);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_TRUE(finishedSuccessfully);
}

inline static String sharedWorkerMainBytes()
{
    return R"TESTRESOURCE(
        <!DOCTYPE html>
        <html>
        <head>
        <script>
        const worker = new SharedWorker("SharedWorker.js");
        worker.port.onmessage = function(event) {
            document.querySelector("code").textContent = event.data;
        };
        worker.port.postMessage({ command: "draw" });
        </script>
        </head>
        <body>
            <h1>SharedWorker</h1>
            The result is: <code></code>
        </body>
        </html>
        )TESTRESOURCE"_s;
}

inline static String sharedWorkerBytes()
{
    return String::fromUTF8(R"TESTRESOURCE(
        onconnect = (event) => {
            const port = event.ports[0];
            port.onmessage = function(event) {
                if (event.data.command !== "draw")
                    return;

                const offscreen = new OffscreenCanvas(200, 200);
                const context = offscreen.getContext("2d");

                const gradient = context.createLinearGradient(0, 0, 200, 0);
                gradient.addColorStop(0, "red");
                gradient.addColorStop(1, "blue");

                context.fillStyle = gradient;
                context.fillRect(0, 0, 200, 200);

                context.fillStyle = "white";
                context.font = "48px serif";
                context.fillText("🙃", 10, 50);

                offscreen.convertToBlob().then(blob => {
                    const reader = new FileReader;
                    reader.onloadend = async () => {
                        const base64 = reader.result.split(",")[1];
                        const bytes = new Uint8Array(atob(base64).split("").map(char => char.charCodeAt(0)));
                        const hashBuffer = await crypto.subtle.digest("SHA-256", bytes);
                        const hashArray = Array.from(new Uint8Array(hashBuffer));
                        const hexRepresentation = hashArray.map(byte => byte.toString(16).padStart(2, "0")).join("");
                        port.postMessage(hexRepresentation);
                    };
                    reader.readAsDataURL(blob);
                });
            };
        };
        )TESTRESOURCE");
}

TEST(AdvancedPrivacyProtections, NoiseInjectionForOffscreenCanvasInSharedWorker)
{
    auto server = HTTPServer { {
        { "/"_s, { sharedWorkerMainBytes() } },
        { "/SharedWorker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, sharedWorkerBytes() } }
    } };

    auto computeHash = [&](TestWKWebView *webView) {
        [webView synchronouslyLoadRequest:server.request("/"_s)];

        RetainPtr<NSString> result;
        Util::waitForConditionWithLogging([&] {
            result = [webView stringByEvaluatingJavaScript:@"document.querySelector('code').textContent"];
            return [result length] > 0;
        }, 3, @"Failed to compute hash using OffscreenCanvas.");
        return result.autorelease();
    };

    RetainPtr hashWithoutNoiseInjectionInEphemeralStore = computeHash(createWebViewWithAdvancedPrivacyProtections(NO, nil, WKWebsiteDataStore.nonPersistentDataStore).get());
    RetainPtr hashWithoutNoiseInjectionInDefaultStore = computeHash(createWebViewWithAdvancedPrivacyProtections(NO, nil, WKWebsiteDataStore.defaultDataStore).get());
    RetainPtr hashWithNoiseInjectionInEphemeralStore = computeHash(createWebViewWithAdvancedPrivacyProtections(YES, nil, WKWebsiteDataStore.nonPersistentDataStore).get());
    EXPECT_NOT_NULL(hashWithoutNoiseInjectionInEphemeralStore);
    EXPECT_NOT_NULL(hashWithoutNoiseInjectionInDefaultStore);
    EXPECT_NOT_NULL(hashWithNoiseInjectionInEphemeralStore);
    EXPECT_FALSE([hashWithNoiseInjectionInEphemeralStore isEqual:hashWithoutNoiseInjectionInDefaultStore.get()]);
    EXPECT_FALSE([hashWithNoiseInjectionInEphemeralStore isEqual:hashWithoutNoiseInjectionInEphemeralStore.get()]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
