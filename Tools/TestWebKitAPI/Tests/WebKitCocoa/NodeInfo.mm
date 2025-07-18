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
#import "TestNavigationDelegate.h"
#import "TestScriptMessageHandler.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKNodeInfo.h>

namespace TestWebKitAPI {

TEST(NodeInfo, Basic)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id=onlyframe src='https://webkit.org/webkit'></iframe><div id=onlydiv></div>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    worldConfiguration.get().allowNodeInfo = YES;
    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];

    __block bool done { false };
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(onlyframe)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:_WKNodeInfo.class]);
        [result contentFrameInfo:^(WKFrameInfo *info) {
            EXPECT_WK_STREQ(info.request.URL.absoluteString, "https://webkit.org/webkit");
            done = true;
        }];
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(onlydiv)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:_WKNodeInfo.class]);
        EXPECT_NULL(error);
        [result contentFrameInfo:^(WKFrameInfo *info) {
            EXPECT_NULL(info);
            done = true;
        }];
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(5)" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.webkit.createNodeInfo(document.createTextNode('hi'))" inFrame:nil inContentWorld:world.get() completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:_WKNodeInfo.class]);
        EXPECT_NULL(error);
        [result contentFrameInfo:^(WKFrameInfo *info) {
            EXPECT_NULL(info);
            done = true;
        }];
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.WebKitNodeInfo" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_NULL(error);
        done = true;
    }];
    Util::run(&done);
}

}
