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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "PlatformUtilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKFeature.h>

namespace TestWebKitAPI {

// LocalNetworkAccessEnabled has no dedicated Obj-C setter, so flip it via the generic _WKFeature list.
static void setLocalNetworkAccessEnabledForConfiguration(WKWebViewConfiguration *configuration, BOOL enabled)
{
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"LocalNetworkAccessEnabled"])
            [configuration.preferences _setEnabled:enabled forFeature:feature];
    }
}

static RetainPtr<TestWKWebView> createWebViewForLocalNetworkAccessTesting(BOOL localNetworkAccessEnabled)
{
    // WebProcessPlugInWithInternals is what makes `window.internals` available to page script here.
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    setLocalNetworkAccessEnabledForConfiguration(configuration.get(), localNetworkAccessEnabled);
    // Without _allowTestOnlyIPC, NetworkProcess treats SetLocalNetworkAccessPermissionForTesting as
    // an invalid message and terminates the WebContent process instead of servicing it.
    configuration.get()._allowTestOnlyIPC = YES;
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
}

static constexpr auto crossOriginLoopbackFetchPageBytes = R"HTMLRESOURCE(
<script>
async function doFetch(targetOrigin)
{
    await fetch(targetOrigin + "/target.txt", { mode: "no-cors" })
        .then(() => window.fetchResult = "success")
        .catch(e => window.fetchResult = "error: " + e.message);
}
</script>
)HTMLRESOURCE"_s;

static String waitForFetchResult(TestWKWebView *webView)
{
    Util::waitForConditionWithLogging([&] -> bool {
        return [[webView stringByEvaluatingJavaScript:@"window.fetchResult || ''"] length] > 0;
    }, 10, @"Timed out waiting for fetch result.");
    return String { [webView stringByEvaluatingJavaScript:@"window.fetchResult"] };
}

TEST(LocalNetworkAccessTests, DisabledFeatureAllowsLoopbackFetchUnconditionally)
{
    auto server = HTTPServer({
        { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(NO);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost("/index.html"_s)];

    RetainPtr targetOrigin = server.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

TEST(LocalNetworkAccessTests, CrossOriginLoopbackFetchBlockedWithoutGrant)
{
    // The page is loaded via requestWithLocalhost so its origin (http://localhost:<port>) differs
    // from the fetch target (http://127.0.0.1:<port>), even though both resolve to the loopback
    // interface -- otherwise the same-origin exemption would trivially allow the request.
    auto server = HTTPServer({
        { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(YES);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost("/index.html"_s)];

    RetainPtr targetOrigin = server.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));
}

TEST(LocalNetworkAccessTests, CrossOriginLoopbackFetchSucceedsWithGrant)
{
    auto server = HTTPServer({
        { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(YES);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost("/index.html"_s)];

    NSString *pageOrigin = [webView stringByEvaluatingJavaScript:@"location.origin"];
    NSString *grantScript = [NSString stringWithFormat:@"(async () => { await internals.setLocalNetworkAccessPermissionForTesting('%@', 'loopback', 'granted'); window.grantComplete = true; })(); undefined", pageOrigin];
    [webView objectByEvaluatingJavaScript:grantScript];
    Util::waitForConditionWithLogging([&] -> bool {
        return [[webView objectByEvaluatingJavaScript:@"window.grantComplete || false"] boolValue];
    }, 10, @"Timed out waiting for permission grant.");

    RetainPtr targetOrigin = server.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

TEST(LocalNetworkAccessTests, SameOriginTargetAddressSpaceFetchSucceedsWithoutGrant)
{
    // This exemption is unconditional and doesn't consult the permission stub, unlike the
    // cross-origin permission-gated path covered by the two tests above.
    auto server = HTTPServer({
        { "/index.html"_s, { "<script>window.pageLoaded = true;</script>"_s } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(YES);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    NSString *fetchScript =
        @"(async () => {"
        "  try {"
        "    await fetch(location.origin + '/target.txt', { targetAddressSpace: 'loopback' });"
        "    window.fetchResult = 'success';"
        "  } catch (e) {"
        "    window.fetchResult = 'error: ' + e.message;"
        "  }"
        "})(); undefined";
    [webView objectByEvaluatingJavaScript:fetchScript];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

} // namespace TestWebKitAPI
