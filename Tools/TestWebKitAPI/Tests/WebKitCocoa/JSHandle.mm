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
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKJSHandle.h>

namespace TestWebKitAPI {

static WKFrameInfo *getWindowFrameInfo(RetainPtr<_WKJSHandle> node)
{
    EXPECT_TRUE([node isKindOfClass:_WKJSHandle.class]);
    __block RetainPtr<WKFrameInfo> frame;
    __block bool done { false };
    [node windowFrameInfo:^(WKFrameInfo *info) {
        frame = info;
        done = true;
    }];
    Util::run(&done);
    return frame.autorelease();
}

TEST(JSHandle, Basic)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id=onlyframe src='https://webkit.org/webkit'></iframe><div id=onlydiv></div>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowJSHandleCreation = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    RetainPtr<id> result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(onlyframe.contentWindow)" inFrame:nil inContentWorld:world.get()];
    EXPECT_WK_STREQ(getWindowFrameInfo(result).request.URL.absoluteString, "https://webkit.org/webkit");
    RetainPtr<_WKJSHandle> iframeRef = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(onlyframe)" inFrame:nil inContentWorld:world.get()];

    result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(onlydiv)" inFrame:nil inContentWorld:world.get()];
    EXPECT_NULL(getWindowFrameInfo(result));
    {
        __block bool done { false };
        [webView evaluateJavaScript:@"window.webkit.createJSHandle(5)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
            EXPECT_NULL(result);
            EXPECT_NOT_NULL(error);
            done = true;
        }];
        Util::run(&done);
    }
    result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(document.createTextNode('hi'))" inFrame:nil inContentWorld:world.get()];
    EXPECT_NULL(getWindowFrameInfo(result));
    EXPECT_EQ([result world], world.get());
    result = [webView objectByEvaluatingJavaScript:@"window.WebKitJSHandle"];
    EXPECT_NULL(result);

    result = [webView objectByCallingAsyncFunction:@"return {'arg':window.webkit.createJSHandle(onlydiv)}" withArguments:nil inFrame:nil inContentWorld:world.get()];
    result = [webView objectByCallingAsyncFunction:@"return arg.outerHTML" withArguments:result.get() inFrame:nil inContentWorld:world.get()];
    EXPECT_WK_STREQ(result.get(), "<div id=\"onlydiv\"></div>");

    result = [webView objectByCallingAsyncFunction:@"return n === undefined" withArguments:@{ @"n" : iframeRef.get() } inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld];
    EXPECT_EQ(result.get(), @YES);

    result = [webView objectByCallingAsyncFunction:@"return n.id" withArguments:@{ @"n" : iframeRef.get() } inFrame:nil inContentWorld:world.get()];
    EXPECT_WK_STREQ(result.get(), "onlyframe");

    result = [webView objectByEvaluatingJavaScript:@"function returnThirty() { return '30'; }; window.webkit.createJSHandle(returnThirty)" inFrame:nil inContentWorld:world.get()];
    EXPECT_WK_STREQ([webView objectByCallingAsyncFunction:@"return n()" withArguments:@{ @"n" : result.get() } inFrame:nil inContentWorld:world.get()], "30");

    result = [webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(window.parent)" inFrame:[webView firstChildFrame] inContentWorld:world.get()];
    EXPECT_WK_STREQ(getWindowFrameInfo(result).request.URL.absoluteString, "https://example.com/example");
}

TEST(JSHandle, Equality)
{
    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowJSHandleCreation = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    RetainPtr webView = adoptNS([TestWKWebView new]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"let a = {'key':'value'}; let b = window.webkit.createJSHandle(a); let c = window.webkit.createJSHandle(a); b === c" inFrame:nil inContentWorld:world.get()] boolValue]);

    _WKJSHandle *b = [webView objectByEvaluatingJavaScript:@"b" inFrame:nil inContentWorld:world.get()];
    _WKJSHandle *c = [webView objectByEvaluatingJavaScript:@"c" inFrame:nil inContentWorld:world.get()];
    _WKJSHandle *d = [webView objectByEvaluatingJavaScript:@"let d = {}; window.webkit.createJSHandle(d)" inFrame:nil inContentWorld:world.get()];
    EXPECT_TRUE([b isKindOfClass:_WKJSHandle.class]);
    EXPECT_TRUE([c isKindOfClass:_WKJSHandle.class]);
    EXPECT_TRUE([d isKindOfClass:_WKJSHandle.class]);
    EXPECT_NE(b, c);
    EXPECT_NE(b, d);
    EXPECT_NE(c, d);
    EXPECT_TRUE([b isEqual:c]);
    EXPECT_FALSE([b isEqual:d]);

    NSMutableArray<_WKJSHandle *> *array = [NSMutableArray arrayWithCapacity:2];
    [array addObject:b];
    [array addObject:d];
    [array addObject:c];
    EXPECT_EQ(array.count, 3u);
    [array removeObject:b];
    EXPECT_EQ(array.count, 1u);

    NSSet<_WKJSHandle *> *set = [NSSet setWithObjects:b, c, d, nil];
    EXPECT_EQ(set.count, 2u);

    NSDictionary<NSString *, _WKJSHandle *> *dictionary = @{
        @"b" : b,
        @"c" : c,
        @"d" : d
    };
    EXPECT_EQ(dictionary[@"b"], b);
    EXPECT_EQ(dictionary[@"c"], c);
    EXPECT_EQ(dictionary[@"d"], d);
    EXPECT_EQ(dictionary.count, 3u);

    EXPECT_WK_STREQ([webView objectByCallingAsyncFunction:@"return b.key" withArguments:@{ @"b" : b } inFrame:nil inContentWorld:world.get()], "value");

    worldConfiguration.get().name = @"otherworldly";
    RetainPtr otherWorld = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];
    EXPECT_NE(world.get(), otherWorld.get());
    EXPECT_TRUE([[webView objectByCallingAsyncFunction:@"return b === undefined" withArguments:@{ @"b" : b } inFrame:nil inContentWorld:otherWorld.get()] boolValue]);
}

TEST(JSHandle, WebpagePreferences)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:server.httpsProxyConfiguration()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    __block bool allow = false;
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._allowsJSHandleCreationInPageWorld = allow;
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]];
    NSString *checkClass = @"window.WebKitJSHandle === undefined";
    NSString *checkWindowWebKit = @"window.webkit === undefined || window.webkit.createJSHandle === undefined";

    [webView loadRequest:request];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:checkClass] boolValue]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:checkWindowWebKit] boolValue]);

    allow = true;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:checkClass] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:checkWindowWebKit] boolValue]);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.webkit.createJSHandle(()=>{})"] world], WKContentWorld.pageWorld);

    allow = false;
    [webView loadRequest:request];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:checkClass] boolValue]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:checkWindowWebKit] boolValue]);

    allow = true;
    [webView loadRequest:request];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:checkClass] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:checkWindowWebKit] boolValue]);
}

TEST(JSHandle, Reuse)
{
    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowJSHandleCreation = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    RetainPtr webView = adoptNS([TestWKWebView new]);
    @autoreleasepool {
        [webView objectByEvaluatingJavaScript:@"let f = window.webkit.createJSHandle(()=>{ return 42; }); f" inFrame:nil inContentWorld:world.get()];
    }
    _WKJSHandle *fun = [webView objectByEvaluatingJavaScript:@"f" inFrame:nil inContentWorld:world.get()];
    EXPECT_TRUE([[webView objectByCallingAsyncFunction:@"return fun()" withArguments:@{ @"fun":fun } inFrame:nil inContentWorld:world.get()] isEqual:@42]);
}

}
