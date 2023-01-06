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
#import "HTTPServer.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <wtf/BlockPtr.h>

namespace TestWebKitAPI {

static void enableSiteIsolation(WKWebViewConfiguration *configuration)
{
    auto preferences = [configuration preferences];
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"SiteIsolationEnabled"]) {
            [preferences _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }
}

TEST(SiteIsolation, LoadingCallbacksAndPostMessage)
{
    auto exampleHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('parent frame received ' + event.data)"
    "    }, false);"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*');"
    "        setTimeout(() => { alert('postMessage failed') }, 1000)"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        parent.window.postMessage('pong', '*')"
    "    }, false)"
    "</script>"_s;

    bool finishedLoading { false };
    size_t framesCommitted { 0 };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse(exampleHTML).serialize());
                continue;
            }
            if (path == "/webkit"_s) {
                size_t contentLength = 2000000 + webkitHTML.length();
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Length: "_s, contentLength, "\r\n\r\n"_s));
                EXPECT_FALSE(finishedLoading);
                Util::runFor(Seconds(0.1));
                EXPECT_FALSE(finishedLoading);
                EXPECT_EQ(framesCommitted, 1u);
                co_await connection.awaitableSend(webkitHTML);
                co_await connection.awaitableSend(Vector<uint8_t>(1000000, ' '));
                while (framesCommitted < 2)
                    Util::spinRunLoop();
                Util::runFor(Seconds(0.1));
                EXPECT_FALSE(finishedLoading);
                co_await connection.awaitableSend(Vector<uint8_t>(1000000, ' '));
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    navigationDelegate.get().didCommitLoadWithRequestInFrame = makeBlockPtr([&](WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo) {
        EXPECT_TRUE(frameInfo.isMainFrame); // FIXME: The second frame should not report that it is the main frame.
        framesCommitted++;
    }).get();

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    finishedLoading = true;

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "postMessage failed"); // FIXME: Hook up postMessage and this should become "parent frame received pong".

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        pid_t mainFramePid = mainFrame._processIdentifier;
        pid_t childFramePid = mainFrame.childFrames.firstObject._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, NavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>setTimeout(() => { alert('navigating to same domain failed') }, 1000)</script>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://example.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "navigating to same domain failed"); // FIXME: This should succeed.

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame._processIdentifier;
        pid_t childFramePid = childFrame._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid); // FIXME: These should be equal.
        EXPECT_WK_STREQ(mainFrame.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.securityOrigin.host, "webkit.org"); // FIXME: This should be example.com like it is without site isolation.
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, ParentNavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://example.com/example_subframe' }</script>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    // FIXME: call enableSiteIsolation to make this actually use site isolation.

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame._processIdentifier;
        pid_t childFramePid = childFrame._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_EQ(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.securityOrigin.host, "example.com");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, IframeNavigatesSelfWithoutChangingOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>window.location='/webkit_second'</script>"_s } },
        { "/webkit_second"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame._processIdentifier;
        pid_t childFramePid = childFrame._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);
}

}
