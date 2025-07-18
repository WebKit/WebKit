/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#if ENABLE(SCRIPT_TRACKING_PRIVACY_PROTECTIONS)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "UserInterfaceSwizzler.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Seconds.h>
#import <wtf/Vector.h>
#import <wtf/text/MakeString.h>

#import <pal/cocoa/WebPrivacySoftLink.h>

@interface WKWebsiteDataStore (ScriptTrackingPrivacyTests)
- (void)deleteAllCookies;
@property (nonatomic, readonly) NSArray<NSHTTPCookie *> *allCookies;
@end

@implementation WKWebsiteDataStore (ScriptTrackingPrivacyTests)

- (void)deleteAllCookies
{
    __block bool done = false;
    [self removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeCookies] modifiedSince:NSDate.distantPast completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (NSArray<NSHTTPCookie *> *)allCookies
{
    __block RetainPtr<NSArray<NSHTTPCookie *>> result;
    __block bool done = false;
    [self.httpCookieStore getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
        result = cookies;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

@interface TestWPFingerprintingScript : NSObject
- (instancetype)initWithHost:(NSString *)host isFirstParty:(BOOL)firstParty isTopDomain:(BOOL)topDomain allowedCategories:(WPScriptAccessCategories)allowedCategories;
@property (nonatomic, readonly) NSString *host;
@property (nonatomic, readonly, getter=isFirstParty) BOOL firstParty;
@property (nonatomic, readonly, getter=isTopDomain) BOOL topDomain;
@property (nonatomic, readonly) WPScriptAccessCategories allowedCategories;
@end

@implementation TestWPFingerprintingScript {
    RetainPtr<NSString> _host;
}

- (instancetype)initWithHost:(NSString *)host isFirstParty:(BOOL)firstParty isTopDomain:(BOOL)topDomain allowedCategories:(WPScriptAccessCategories)allowedCategories
{
    if (!(self = [super init]))
        return nil;

    _host = adoptNS([host copy]);
    _firstParty = firstParty;
    _topDomain = topDomain;
    _allowedCategories = allowedCategories;
    return self;
}

- (NSString *)host
{
    return _host.get();
}

@end

namespace TestWebKitAPI {

static IMP makeFingerprintingScriptsRequestHandler(NSArray<NSString *> *hostNames, Vector<WPScriptAccessCategories>&& allowedCategories)
{
    return imp_implementationWithBlock([hostNames = RetainPtr { hostNames }, allowedCategories = WTFMove(allowedCategories)](WPResources *, WPResourceRequestOptions *, void(^completion)(NSArray<WPFingerprintingScript *> *, NSError *)) mutable {
        RunLoop::mainSingleton().dispatch([hostNames = WTFMove(hostNames), allowedCategories = WTFMove(allowedCategories), completion = makeBlockPtr(completion)] mutable {
            RetainPtr scripts = [NSMutableArray arrayWithCapacity:[hostNames count]];
            size_t index = 0;
            for (NSString *host in hostNames.get()) {
                RetainPtr script = adoptNS([[TestWPFingerprintingScript alloc]
                    initWithHost:host
                    isFirstParty:NO
                    isTopDomain:NO
                    allowedCategories:allowedCategories.isEmpty() ? WPScriptAccessCategoryNone : allowedCategories[index]]);
                [scripts addObject:(WPFingerprintingScript *)script.get()];
                index++;
            }
            completion(scripts.get(), nil);
        });
    });
}

class FingerprintingScriptsRequestSwizzler {
    WTF_MAKE_NONCOPYABLE(FingerprintingScriptsRequestSwizzler);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(FingerprintingScriptsRequestSwizzler);
public:
    FingerprintingScriptsRequestSwizzler(NSArray<NSString *> *hosts, Vector<WPScriptAccessCategories>&& allowedCategories = { })
    {
        m_swizzler = makeUnique<InstanceMethodSwizzler>(
            PAL::getWPResourcesClass(),
            @selector(requestFingerprintingScripts:completionHandler:),
            makeFingerprintingScriptsRequestHandler(hosts, WTFMove(allowedCategories))
        );
    }

private:
    std::unique_ptr<InstanceMethodSwizzler> m_swizzler;
};

static bool supportsFingerprintingScriptRequests()
{
    return PAL::isWebPrivacyFrameworkAvailable()
        && [PAL::getWPResourcesClass() instancesRespondToSelector:@selector(requestFingerprintingScripts:completionHandler:)];
}

static RetainPtr<TestWKWebView> setUpWebViewForFingerprintingTests(NSString *pageURLString, id<WKUIDelegate> uiDelegate, NSDictionary<NSString *, NSString *> *responseData,
    NSString *referrer = @"https://webkit.org", _WKWebsiteNetworkConnectionIntegrityPolicy policies = _WKWebsiteNetworkConnectionIntegrityPolicyNone)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"ScriptTrackingPrivacyProtectionsEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr dataStore = [WKWebsiteDataStore defaultDataStore];
    [dataStore _setResourceLoadStatisticsEnabled:YES];
    [configuration setWebsiteDataStore:dataStore.get()];
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    [[configuration defaultWebpagePreferences] _setNetworkConnectionIntegrityPolicy:policies];

    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:[responseData = retainPtr(responseData)](WKWebView *, id<WKURLSchemeTask> task) {
        NSURL *requestedURL = task.request.URL;
        NSString *result = [responseData objectForKey:requestedURL.absoluteString] ?: @"";
        if (!result) {
            [task didFailWithError:[NSError errorWithDomain:@"TestWebKitAPI" code:1 userInfo:nil]];
            return;
        }

        NSString *pathExtension = requestedURL.pathExtension;
        NSString *type = @"text/plain";
        if ([pathExtension isEqualToString:@"js"])
            type = @"text/javascript";
        else if ([pathExtension isEqualToString:@"html"])
            type = @"text/html";
        RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:requestedURL MIMEType:type expectedContentLength:[result length] textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[result dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];

    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:configuration.get()]);
    [webView setUIDelegate:uiDelegate];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    if (!pageURLString)
        return webView;

    RetainPtr finalRequest = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:pageURLString]];

    if (referrer)
        [finalRequest setValue:referrer forHTTPHeaderField:@"referer"];

    [webView synchronouslyLoadRequest:finalRequest.get()];

    return webView;
}

static RetainPtr<TestWKWebView> setUpWebViewForFingerprintingTests(NSString *pageURLString, NSDictionary<NSString *, NSString *> *responseData,
    NSString *referrer = @"https://webkit.org", _WKWebsiteNetworkConnectionIntegrityPolicy policies = _WKWebsiteNetworkConnectionIntegrityPolicyNone)
{
    return setUpWebViewForFingerprintingTests(pageURLString, nil, responseData, referrer, policies);
}

static NSString *getBundleResourceAsText(NSString *filename, NSString *extension)
{
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:filename withExtension:extension];
    return [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:nil];
}

static constexpr auto simpleIndexHTML = R"markup(
    <!DOCTYPE html>
    <html>
        <head>
            <script src="test://top-domain.org/script.js"></script>
        </head>
        <body>
            <script src="test://tainted.net/script.js"></script>
            <script src="test://pure.com/script.js"></script>
        </body>
    </html>
)markup"_s;

static constexpr auto formFieldIndexHTML = R"markup(
    <!DOCTYPE html>
    <html>
        <head>
            <script src="test://top-domain.org/script.js"></script>
        </head>
        <body>
            <div id="bodyTop">Welcome</div>
            <form>
                <input type="email" name="emailField" id="emailField" value="emailFieldValue" placeholder="my.email@example.com">
                <input type="text" name="textField" id="textField" value="textFieldValue">
                <input type="date" name="dateField" id="dateField" value="1999-12-31">
                <input type="file" name="fileField" id="fileField" value="C:\fakepath\fileFieldValue">
                <input type="month" name="monthField" id="monthField" value="1999-12">
                <input type="password" name="passwordField" id="passwordField" value="passwordFieldValue" placeholder="super_dooper_s3cure">
                <input type="search" name="searchField" id="searchField" value="searchFieldValue">
                <input type="tel" name="telField" id="telField" value="telFieldValue">
                <input type="time" name="timeField" id="timeField" value="00:00:00">
                <input type="url" name="urlField" id="urlField" value="urlFieldValue">
                <input type="week" name="weekField" id="weekField" value="weekFieldValue">
            </form>
            <textarea id="textAreaField">Text Area</textarea>
            <select name="selectField" id="selectField">
                <option value="Primary"></option>
            </select>
            <script src="test://tainted.net/script.js"></script>
            <script src="test://pure.com/script.js"></script>
        </body>
    </html>
)markup"_s;

TEST(ScriptTrackingPrivacyTests, Referrer)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", @{
        @"test://top-domain.org/index.html" : simpleIndexHTML.createNSString().autorelease(),
        @"test://pure.com/script.js" : @"window.referrerForPureScript = document.referrer;",
        @"test://tainted.net/script.js" : @"window.referrerForTaintedScript = document.referrer;"
    });

    EXPECT_WK_STREQ("https://webkit.org/", [webView stringByEvaluatingJavaScript:@"window.referrerForPureScript"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"window.referrerForTaintedScript"]);
}

TEST(ScriptTrackingPrivacyTests, QueryParameters)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html?uid=Hv9U23Hfco08", @{
        @"test://top-domain.org/index.html?uid=Hv9U23Hfco08" : simpleIndexHTML.createNSString().autorelease(),
        @"test://pure.com/script.js" : @"window.urlForPureScript = document.URL;",
        @"test://tainted.net/script.js" : @"window.urlForTaintedScript = document.URL;"
    });

    EXPECT_WK_STREQ("test://top-domain.org/index.html?uid=Hv9U23Hfco08", [webView stringByEvaluatingJavaScript:@"window.urlForPureScript"]);
    EXPECT_WK_STREQ("test://top-domain.org/index.html", [webView stringByEvaluatingJavaScript:@"window.urlForTaintedScript"]);
}

TEST(ScriptTrackingPrivacyTests, Canvas2D)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    NSString *addHashScriptSource = @"fullCanvasHash().then(hash => {"
        "    if (window.hashes)"
        "        window.hashes.push(hash);"
        "    else"
        "        window.hashes = [hash];"
        "})";

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", @{
        @"test://top-domain.org/index.html" : simpleIndexHTML.createNSString().autorelease(),
        @"test://top-domain.org/script.js" : getBundleResourceAsText(@"canvas-fingerprinting", @"js"),
        @"test://pure.com/script.js" : addHashScriptSource,
        @"test://tainted.net/script.js" : addHashScriptSource,
    });

    RetainPtr<NSArray> hashes;
    Util::waitForConditionWithLogging([&] -> bool {
        hashes = [webView objectByEvaluatingJavaScript:@"window.hashes || []"];
        return [hashes count] == 2;
    }, 10, @"Timed out while computing hashes.");

    BOOL hashesAreEqual = [[hashes firstObject] isEqual:[hashes lastObject]];
    EXPECT_FALSE(hashesAreEqual);
    if (hashesAreEqual)
        NSLog(@"FAIL: Expected hashes to be different: %@", [hashes firstObject]);
}

TEST(ScriptTrackingPrivacyTests, AudioSamples)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    NSString *addHashScriptSource = @"testOscillatorCompressorAnalyzer().then(hash => {"
        "    if (window.hashes)"
        "        window.hashes.push(hash);"
        "    else"
        "        window.hashes = [hash];"
        "})";

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", @{
        @"test://top-domain.org/index.html" : simpleIndexHTML.createNSString().autorelease(),
        @"test://top-domain.org/script.js" : getBundleResourceAsText(@"audio-fingerprinting", @"js"),
        @"test://pure.com/script.js" : addHashScriptSource,
        @"test://tainted.net/script.js" : addHashScriptSource,
    });

    RetainPtr<NSArray> hashes;
    Util::waitForConditionWithLogging([&] -> bool {
        hashes = [webView objectByEvaluatingJavaScript:@"window.hashes || []"];
        return [hashes count] == 2;
    }, 10, @"Timed out while computing hashes.");

    BOOL hashesAreEqual = [[hashes firstObject] isEqual:[hashes lastObject]];
    EXPECT_FALSE(hashesAreEqual);
    if (hashesAreEqual)
        NSLog(@"FAIL: Expected hashes to be different: %@", [hashes firstObject]);
}

TEST(ScriptTrackingPrivacyTests, ScreenMetrics)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };
#if PLATFORM(IOS_FAMILY)
    IPadUserInterfaceSwizzler userInterfaceSwizzler;
#endif

    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
#if PLATFORM(MAC)
    [uiDelegate setGetWindowFrameWithCompletionHandler:^(WKWebView *view, void(^completionHandler)(CGRect)) {
        CGRect viewBounds = view.bounds;
        viewBounds.origin = CGPointMake(10, 10);
        viewBounds.size.width += 10;
        viewBounds.size.height += 10;
        completionHandler(viewBounds);
    }];
#endif // PLATFORM(MAC)

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", uiDelegate.get(), @{
        @"test://top-domain.org/index.html" : simpleIndexHTML.createNSString().autorelease(),
        @"test://pure.com/script.js" : @"window.pureInfo = { screenX, screenY, 'screen.width': screen.width, 'screen.height': screen.height, outerWidth, outerHeight }",
        @"test://tainted.net/script.js" : @"window.taintedInfo = { screenX, screenY, 'screen.width': screen.width, 'screen.height': screen.height, outerWidth, outerHeight }"
    });

    NSDictionary<NSString *, NSNumber *> *pureInfo = [webView objectByEvaluatingJavaScript:@"window.pureInfo"];
    NSDictionary<NSString *, NSNumber *> *taintedInfo = [webView objectByEvaluatingJavaScript:@"window.taintedInfo"];
#if PLATFORM(MAC)
    for (NSString *key in pureInfo)
        EXPECT_FALSE([pureInfo[key] isEqual:taintedInfo[key]]);
#else
    UNUSED_PARAM(pureInfo);
#endif
    auto innerWidth = [[webView objectByEvaluatingJavaScript:@"innerWidth"] intValue];
    auto innerHeight = [[webView objectByEvaluatingJavaScript:@"innerHeight"] intValue];
    EXPECT_EQ(0, [taintedInfo[@"screenX"] intValue]);
    EXPECT_EQ(0, [taintedInfo[@"screenY"] intValue]);
    EXPECT_EQ(innerWidth, [taintedInfo[@"screen.width"] intValue]);
    EXPECT_EQ(innerHeight, [taintedInfo[@"screen.height"] intValue]);
    EXPECT_EQ(innerWidth, [taintedInfo[@"outerWidth"] intValue]);
    EXPECT_EQ(innerHeight, [taintedInfo[@"outerHeight"] intValue]);
}

TEST(ScriptTrackingPrivacyTests, ScriptWrittenCookies)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    auto makeScriptSource = ^(NSString *pureOrTainted) {
        return [NSString stringWithFormat:@"(function () {"
            "    const date = new Date;"
            "    date.setMonth(date.getMonth() + 1);" // Expire after 1 month.
            "    document.cookie = `%@=%@Value; expires=${date.toUTCString()}`;"
            "})()", pureOrTainted, pureOrTainted];
    };

    RetainPtr webView = setUpWebViewForFingerprintingTests(nil, @{
        @"test://pure.com/script.js" : makeScriptSource(@"pure"),
        @"test://tainted.net/script.js" : makeScriptSource(@"tainted"),
    });

    RetainPtr dataStore = [[webView configuration] websiteDataStore];
    [dataStore deleteAllCookies];

    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:simpleIndexHTML.createNSString().autorelease()];

    BOOL foundPureCookie = NO;
    BOOL foundTaintedCookie = NO;
    RetainPtr currentTime = [NSDate date];
    static constexpr auto oneDayAndTenMinutes = 24_h + 10_min;

    RetainPtr allCookies = [dataStore allCookies];
    for (NSHTTPCookie *cookie in allCookies.get()) {
        NSString *cookieName = cookie.name;
        NSString *cookieValue = cookie.value;
        auto secondsUntilExpiry = [cookie.expiresDate timeIntervalSinceDate:currentTime.get()];
        if ([cookieName isEqualToString:@"tainted"]) {
            foundTaintedCookie = YES;
            EXPECT_LT(secondsUntilExpiry, oneDayAndTenMinutes.seconds());
            EXPECT_WK_STREQ("taintedValue", cookieValue);
            continue;
        }

        if ([cookieName isEqualToString:@"pure"]) {
            foundPureCookie = YES;
            EXPECT_GT(secondsUntilExpiry, oneDayAndTenMinutes.seconds());
            EXPECT_WK_STREQ("pureValue", cookieValue);
            continue;
        }
    }
    EXPECT_TRUE(foundPureCookie);
    EXPECT_TRUE(foundTaintedCookie);
}

TEST(ScriptTrackingPrivacyTests, LocalStorage)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    auto makeScriptSource = ^(NSString *pureOrTainted) {
        return [NSString stringWithFormat:@"localStorage.setItem('%@', 'foo'); window.%@Item = localStorage.getItem('%@')", pureOrTainted, pureOrTainted, pureOrTainted];
    };

    RetainPtr webView = setUpWebViewForFingerprintingTests(nil, @{
        @"test://pure.com/script.js" : makeScriptSource(@"pure"),
        @"test://tainted.net/script.js" : makeScriptSource(@"tainted"),
    });
    RetainPtr request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org"]];
    [webView synchronouslyLoadSimulatedRequest:request.get() responseHTMLString:simpleIndexHTML.createNSString().autorelease()];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"localStorage.getItem('tainted') || ''"]);
    EXPECT_WK_STREQ("foo", [webView stringByEvaluatingJavaScript:@"window.pureItem || ''"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"window.taintedItem || ''"]);
}

TEST(ScriptTrackingPrivacyTests, HardwareConcurrency)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    auto computeHardwareConcurrency = [] {
        RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", @{
            @"test://top-domain.org/index.html" : simpleIndexHTML.createNSString().autorelease(),
            @"test://pure.com/script.js" : @"window.pureValue = navigator.hardwareConcurrency",
            @"test://tainted.net/script.js" : @"window.taintedValue = navigator.hardwareConcurrency",
        });

        return std::pair {
            [[webView objectByEvaluatingJavaScript:@"window.pureValue"] intValue],
            [[webView objectByEvaluatingJavaScript:@"window.taintedValue"] intValue]
        };
    };

    bool observedRandomValue = false;
    for (int i = 0; i < 5; ++i) {
        auto [pureValue, taintedValue] = computeHardwareConcurrency();
        observedRandomValue = pureValue != taintedValue;
        if (observedRandomValue)
            break;
    }
    EXPECT_TRUE(observedRandomValue);
}

TEST(ScriptTrackingPrivacyTests, SpeechSynthesisGetVoices)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", @{
        @"test://top-domain.org/index.html" : simpleIndexHTML.createNSString().autorelease(),
        @"test://top-domain.org/script.js" : @"internals.enableMockSpeechSynthesizer()",
        @"test://pure.com/script.js" : @"window.pureNumberOfVoices = speechSynthesis.getVoices().length",
        @"test://tainted.net/script.js" : @"window.taintedNumberOfVoices = speechSynthesis.getVoices().length",
    });

    auto pureNumberOfVoices = [[webView objectByEvaluatingJavaScript:@"window.pureNumberOfVoices"] unsignedIntValue];
    EXPECT_EQ(pureNumberOfVoices, 3u);

    auto taintedNumberOfVoices = [[webView objectByEvaluatingJavaScript:@"window.taintedNumberOfVoices"] unsignedIntValue];
    EXPECT_EQ(taintedNumberOfVoices, 0u);
}

TEST(ScriptTrackingPrivacyTests, DirectFormFieldAccess)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler { @[ @"tainted.net" ] };

    auto makeScriptSource = ^(NSString *pureOrTainted) {
        return [NSString stringWithFormat:@"var %@InputElements = document.querySelectorAll(\"input\");"
            "var %@InputElementsValues = [];"
            "%@InputElements.forEach((e) => %@InputElementsValues.push(e.value));"
            "var %@BodyTopGetElementByIdInnerHTML = document.getElementById(\"bodyTop\")?.innerHTML;"
            "var %@EmailInputGetElementByIdValue = document.getElementById(\"emailField\")?.value;"
            "var %@TextInputGetElementByIdValue = document.getElementById(\"textField\")?.value;"
            "var %@DateInputGetElementByIdValue = document.getElementById(\"dateField\")?.value;"
            "var %@TextAreaGetElementByIdValue = document.getElementById(\"textAreaField\")?.value;"
            "var %@SelectGetElementByIdValue = document.getElementById(\"selectField\")?.value;"
            , pureOrTainted, pureOrTainted, pureOrTainted, pureOrTainted, pureOrTainted, pureOrTainted, pureOrTainted, pureOrTainted, pureOrTainted, pureOrTainted];
    };

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", @{
        @"test://top-domain.org/index.html" : formFieldIndexHTML.createNSString().autorelease(),
        @"test://pure.com/script.js" : makeScriptSource(@"pure"),
        @"test://tainted.net/script.js" : makeScriptSource(@"tainted"),
    }, @"https://webkit.org", _WKWebsiteNetworkConnectionIntegrityPolicyEnabled);

    Vector formFields { {
        "emailField"_s
        , "textField"_s
        , "dateField"_s
        , "fileField"_s
        , "monthField"_s
        , "passwordField"_s
        , "searchField"_s
        , "telField"_s
        , "timeField"_s
        , "urlField"_s
        , "weekField"_s
        , "textAreaField"_s
        , "selectField"_s } };

    const auto expectedPureValue = [](const auto& field) {
        if (field == "dateField"_s)
            return "1999-12-31"_str;
        if (field == "monthField"_s)
            return "1999-12"_str;
        if (field == "timeField"_s)
            return "00:00:00"_str;
        if (field == "textAreaField"_s)
            return "Text Area"_str;
        if (field == "selectField"_s)
            return "Primary"_str;
        if (field == "fileField"_s)
            return emptyString();
        return makeString(field, "Value"_s);
    };

    auto pureNumberInputElements = [[webView objectByEvaluatingJavaScript:@"pureInputElementsValues.length"] unsignedIntValue];
    EXPECT_EQ(pureNumberInputElements, 11u);
    for (size_t i = 0; i < pureNumberInputElements; ++i) {
        auto pureInputValue = [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"pureInputElementsValues[%zu]", i]];
        EXPECT_WK_STREQ(pureInputValue, expectedPureValue(formFields[i]));
    }

    auto pureBodyTopInputGetElementById = [webView stringByEvaluatingJavaScript:@"pureBodyTopGetElementByIdInnerHTML"];
    EXPECT_WK_STREQ(pureBodyTopInputGetElementById, "Welcome"_s);
    auto pureEmailInputGetElementById = [webView stringByEvaluatingJavaScript:@"pureEmailInputGetElementByIdValue"];
    EXPECT_WK_STREQ(pureEmailInputGetElementById, expectedPureValue("emailField"_s));
    auto pureTextInputGetElementById = [webView stringByEvaluatingJavaScript:@"pureTextInputGetElementByIdValue"];
    EXPECT_WK_STREQ(pureTextInputGetElementById, expectedPureValue("textField"_s));
    auto pureDateInputGetElementById = [webView stringByEvaluatingJavaScript:@"pureDateInputGetElementByIdValue"];
    EXPECT_WK_STREQ(pureDateInputGetElementById, expectedPureValue("dateField"_s));
    auto pureTextAreaGetElementById = [webView stringByEvaluatingJavaScript:@"pureTextAreaGetElementByIdValue"];
    EXPECT_WK_STREQ(pureTextAreaGetElementById, expectedPureValue("textAreaField"_s));
    auto pureSelectGetElementById = [webView stringByEvaluatingJavaScript:@"pureSelectGetElementByIdValue"];
    EXPECT_WK_STREQ(pureSelectGetElementById, expectedPureValue("selectField"_s));

    auto taintedNumberInputElements = [[webView objectByEvaluatingJavaScript:@"taintedInputElements.length"] unsignedIntValue];
    EXPECT_EQ(taintedNumberInputElements, 11u);

    for (size_t i = 0; i < taintedNumberInputElements; ++i) {
        auto taintedInputValue = [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"taintedInputElementsValues[%zu]", i]];
        EXPECT_WK_STREQ(taintedInputValue, emptyString());
    }

    auto taintedBodyTopInputGetElementById = [webView stringByEvaluatingJavaScript:@"taintedBodyTopGetElementByIdInnerHTML"];
    EXPECT_WK_STREQ(taintedBodyTopInputGetElementById, "Welcome"_s);
    auto taintedEmailInputGetElementById = [webView stringByEvaluatingJavaScript:@"taintedEmailInputGetElementByIdValue"];
    EXPECT_WK_STREQ(taintedEmailInputGetElementById, emptyString());
    auto taintedTextInputGetElementById = [webView stringByEvaluatingJavaScript:@"taintedTextInputGetElementByIdValue"];
    EXPECT_WK_STREQ(taintedTextInputGetElementById, emptyString());
    auto taintedDateInputGetElementById = [webView stringByEvaluatingJavaScript:@"taintedDateInputGetElementByIdValue"];
    EXPECT_WK_STREQ(taintedDateInputGetElementById, emptyString());
    auto taintedTextAreaGetElementById = [webView stringByEvaluatingJavaScript:@"taintedTextAreaGetElementByIdValue"];
    EXPECT_WK_STREQ(taintedTextAreaGetElementById, emptyString());
    auto taintedSelectGetElementById = [webView stringByEvaluatingJavaScript:@"taintedSelectGetElementByIdValue"];
    EXPECT_WK_STREQ(taintedSelectGetElementById, emptyString());
}

TEST(ScriptTrackingPrivacyTests, ScriptAccessCategories)
{
    if (!supportsFingerprintingScriptRequests())
        return;

    FingerprintingScriptsRequestSwizzler swizzler {
        @[ @"tainted.net" ],
        { WPScriptAccessCategoryFormControls | WPScriptAccessCategoryQueryParameters }
    };

    auto makeTestScript = ^(NSString *pureOrTainted) {
        return [NSString stringWithFormat:@"(function() {"
            "  window.%@NumberOfVoices = speechSynthesis.getVoices().length;"
            "  window.%@TextFieldValue = document.getElementById('textField').value;"
            "})()", pureOrTainted, pureOrTainted];
    };

    RetainPtr webView = setUpWebViewForFingerprintingTests(@"test://top-domain.org/index.html", @{
        @"test://top-domain.org/index.html" : formFieldIndexHTML.createNSString().autorelease(),
        @"test://top-domain.org/script.js" : @"internals.enableMockSpeechSynthesizer()",
        @"test://pure.com/script.js" : makeTestScript(@"pure"),
        @"test://tainted.net/script.js" : makeTestScript(@"tainted"),
    });

    RetainPtr pureTextFieldValue = [webView stringByEvaluatingJavaScript:@"window.pureTextFieldValue"];
    EXPECT_WK_STREQ(pureTextFieldValue.get(), @"textFieldValue");

    RetainPtr taintedTextFieldValue = [webView stringByEvaluatingJavaScript:@"window.taintedTextFieldValue"];
    EXPECT_WK_STREQ(taintedTextFieldValue.get(), @"textFieldValue");

    auto pureNumberOfVoices = [[webView objectByEvaluatingJavaScript:@"window.pureNumberOfVoices"] intValue];
    EXPECT_EQ(pureNumberOfVoices, 3);

    auto taintedNumberOfVoices = [[webView objectByEvaluatingJavaScript:@"window.taintedNumberOfVoices"] intValue];
    EXPECT_EQ(taintedNumberOfVoices, 0);
}

} // namespace TestWebKitAPI

#endif // ENABLE(SCRIPT_TRACKING_PRIVACY_PROTECTIONS)
