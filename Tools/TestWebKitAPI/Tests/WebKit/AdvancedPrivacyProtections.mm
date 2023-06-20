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
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#endif

#import <pal/cocoa/WebPrivacySoftLink.h>

@interface WPLinkFilteringData (TestSupport)
- (instancetype)initWithQueryParameters:(NSArray<NSString *> *)queryParameters;
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

#if PLATFORM(MAC)

// Note: this testing strategy makes a couple of assumptions:
// 1. The network process hasn't already died and allowed the system to reuse the same PID.
// 2. The API test did not take more than ~120 seconds to run.
- (NSArray<NSString *> *)collectLogsForNewConnections
{
    auto predicate = [NSString stringWithFormat:@"subsystem == 'com.apple.network'"
        " AND category == 'connection'"
        " AND eventMessage endswith 'start'"
        " AND processIdentifier == %d", self._networkProcessIdentifier];
    RetainPtr pipe = [NSPipe pipe];
    // FIXME: This is currently reliant on `NSTask`, which is absent on iOS. We should find a way to
    // make this helper work on both platforms.
    auto task = adoptNS([NSTask new]);
    [task setLaunchPath:@"/usr/bin/log"];
    [task setArguments:@[ @"show", @"--last", @"2m", @"--style", @"json", @"--predicate", predicate ]];
    [task setStandardOutput:pipe.get()];
    [task launch];
    [task waitUntilExit];

    auto rawData = [pipe fileHandleForReading].availableData;
    auto messages = [NSMutableArray<NSString *> array];
    for (id messageData in dynamic_objc_cast<NSArray>([NSJSONSerialization JSONObjectWithData:rawData options:0 error:nil]))
        [messages addObject:dynamic_objc_cast<NSString>([messageData objectForKey:@"eventMessage"])];
    return messages;
}

#endif // PLATFORM(MAC)

@end

namespace TestWebKitAPI {

static IMP makeQueryParameterRequestHandler(NSArray<NSString *> *parameters, bool& didHandleRequest)
{
    return imp_implementationWithBlock([&didHandleRequest, parameters = RetainPtr { parameters }](WPResources *, WPResourceRequestOptions *, void(^completion)(WPLinkFilteringData *, NSError *)) mutable {
        RunLoop::main().dispatch([&didHandleRequest, parameters = WTFMove(parameters), completion = makeBlockPtr(completion)]() mutable {
            auto data = adoptNS([PAL::allocWPLinkFilteringDataInstance() initWithQueryParameters:parameters.get()]);
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
    QueryParameterRequestSwizzler(NSArray<NSString *> *parameters)
    {
        update(parameters);
    }

    void update(NSArray<NSString *> *parameters)
    {
        m_didHandleRequest = false;
        // Ensure that the previous swizzler is destroyed before creating the new one.
        m_swizzler = nullptr;
        m_swizzler = makeUnique<InstanceMethodSwizzler>(PAL::getWPResourcesClass(), @selector(requestLinkFilteringData:completionHandler:), makeQueryParameterRequestHandler(parameters, m_didHandleRequest));
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

static RetainPtr<TestWKWebView> createWebViewWithAdvancedPrivacyProtections(BOOL enabled = YES, RetainPtr<WKWebpagePreferences>&& preferences = { }, WKWebsiteDataStore *dataStore = nil)
{
    auto store = dataStore ?: WKWebsiteDataStore.nonPersistentDataStore;
    store._resourceLoadStatisticsEnabled = YES;

    auto policies = enabled
        ? _WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry | _WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicyRequestValidation | _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters
        : _WKWebsiteNetworkConnectionIntegrityPolicyNone;

    preferences = preferences ?: adoptNS([WKWebpagePreferences new]);
    [preferences _setNetworkConnectionIntegrityPolicy:policies];

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:store];
    [configuration setDefaultWebpagePreferences:preferences.get()];

    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
}

TEST(AdvancedPrivacyProtections, DoNotRemoveTrackingQueryParametersWhenPrivacyProtectionsAreDisabled)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections(NO);
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenNavigating)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ] };

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenNavigatingWithContentBlockersDisabled)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ] };

    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setContentBlockersEnabled:NO];
    auto webView = createWebViewWithAdvancedPrivacyProtections(YES, preferences.get());
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

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
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ] };

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
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ] };
    copyURLWithQueryParameters();

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];
    [webView paste:nil];

    EXPECT_WK_STREQ("https://webkit.org/?garply=hello", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("https://webkit.org/?garply=hello", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenPastingURL)
{
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ] };
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
    QueryParameterRequestSwizzler swizzler { @[ @"foo" ] };
    auto url = [NSURL URLWithString:@"https://bundle-file/simple.html?foo=10&garply=20&bar=30&baz=40"];

    auto webView = createWebViewWithAdvancedPrivacyProtections();
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20&bar=30&baz=40", [webView URL].absoluteString);

    swizzler.update(@[ @"foo", @"bar", @"baz" ]);
    swizzler.waitUntilDidHandleRequest();

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    EXPECT_WK_STREQ("https://bundle-file/simple.html?garply=20", [webView URL].absoluteString);
}

TEST(AdvancedPrivacyProtections, RemoveTrackingQueryParametersWhenDecidingNavigationPolicy)
{
    [TestProtocol registerWithScheme:@"https"];
    QueryParameterRequestSwizzler swizzler { @[ @"foo", @"bar", @"baz" ] };

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
    QueryParameterRequestSwizzler blockListSwizzler { @[ @"blockThis" ] };

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
    QueryParameterRequestSwizzler blockListSwizzler { @[ @"blockMe" ] };

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
    QueryParameterRequestSwizzler blockListSwizzler { @[ @"blockMe" ] };

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

#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

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
    EXPECT_EQ([logMessages count], 2U);
    for (NSString *message : logMessages)
        EXPECT_TRUE([message containsString:@"enhanced privacy"]);
#endif
}

#endif // HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

static RetainPtr<TestWKWebView> webViewAfterCrossSiteNavigationWithReducedPrivacy(NSString *initialURLString)
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
        [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnabled | _WKWebsiteNetworkConnectionIntegrityPolicySanitizeLookalikeCharacters];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    }];

    [webView evaluateJavaScript:@"document.querySelector('a').click()" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    return webView;
}

TEST(AdvancedPrivacyProtections, DoNotHideReferrerAfterReducingPrivacyProtections)
{
    HTTPServer server({
        { "/index2.html"_s, { "<script>window.result = document.referrer;</script>"_s } },
    }, HTTPServer::Protocol::Http);

    server.addResponse("/index1.html"_s, { "<a href='http://127.0.0.1:"_s + server.port() + "/index2.html'>Link</a>"_s });

    auto webView = webViewAfterCrossSiteNavigationWithReducedPrivacy(makeString("http://localhost:"_s, server.port(), "/index1.html"_s));

    NSString *result = [webView objectByEvaluatingJavaScript:@"window.result"];
    NSString *expectedReferrer = [NSString stringWithFormat:@"http://localhost:%d/", server.port()];
    EXPECT_WK_STREQ(expectedReferrer, result);
}

TEST(AdvancedPrivacyProtections, DoNotRemoveTrackingParametersAfterReducingPrivacyProtections)
{
    QueryParameterRequestSwizzler blockListSwizzler { @[ @"someID" ] };

    auto pathAndQuery = "/index2.html?someID=123"_s;
    HTTPServer server({
        { pathAndQuery, { "<body>Destination</body>"_s } },
    }, HTTPServer::Protocol::Http);

    server.addResponse("/index1.html"_s, makeString("<a href='http://127.0.0.1:"_s, server.port(), pathAndQuery, "'>Link</a>"_s));

    auto webView = webViewAfterCrossSiteNavigationWithReducedPrivacy(makeString("http://localhost:"_s, server.port(), "/index1.html"_s));

    auto checkForExpectedQueryParameters = [](NSURL *url) {
        auto parameters = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO].queryItems;
        EXPECT_EQ(1U, parameters.count);
        EXPECT_WK_STREQ("someID", parameters.firstObject.name);
        EXPECT_WK_STREQ("123", parameters.firstObject.value);
    };

    auto documentURLString = [webView stringByEvaluatingJavaScript:@"document.URL"];

    checkForExpectedQueryParameters([NSURL URLWithString:documentURLString]);
    checkForExpectedQueryParameters([webView URL]);
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

    runTestWithScreenSize(CGSizeMake(393, 852), CGSizeMake(414, 736));
    runTestWithScreenSize(CGSizeMake(320,  568), CGSizeMake(320,  568));
    runTestWithScreenSize(CGSizeMake(440,  900), CGSizeMake(414,  736));
}

#endif // PLATFORM(IOS_FAMILY)

TEST(AdvancedPrivacyProtections, AddNoiseToWebAudioAPIs)
{
    auto testURL = [NSBundle.mainBundle URLForResource:@"audio-fingerprinting" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto resourcesURL = [NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"TestWebKitAPI.resources"];

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
}

TEST(AdvancedPrivacyProtections, VerifyHashFromNoisyCanvas2DAPI)
{
    auto testURL = [NSBundle.mainBundle URLForResource:@"canvas-fingerprinting" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto resourcesURL = [NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"TestWebKitAPI.resources"];

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

TEST(AdvancedPrivacyProtections, VerifyPixelsFromNoisyCanvas2DAPI)
{
    constexpr auto zeroPrefix = 380;
    constexpr auto channelsPerPixel = 4;

    auto testURL = [NSBundle.mainBundle URLForResource:@"canvas-fingerprinting" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto resourcesURL = [NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"TestWebKitAPI.resources"];

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
    auto testURL = [NSBundle.mainBundle URLForResource:@"canvas-fingerprinting" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    auto resourcesURL = [NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"TestWebKitAPI.resources"];

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

} // namespace TestWebKitAPI

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
