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
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/BlockPtr.h>

namespace TestWebKitAPI {

static void enableSiteIsolation(WKWebViewConfiguration *configuration)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"SiteIsolationEnabled"]) {
            [preferences _setEnabled:YES forFeature:feature];
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
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        parent.window.postMessage(event.data + 'pong', { 'targetOrigin' : '*' });"
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

                co_await connection.awaitableSend(webkitHTML);
                co_await connection.awaitableSend(Vector<uint8_t>(1000000, ' '));

                while (framesCommitted < 2)
                    Util::spinRunLoop();
                Util::runFor(Seconds(0.5));
                EXPECT_EQ(framesCommitted, 2u);

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
        NSString *url = frameInfo.request.URL.absoluteString;
        switch (++framesCommitted) {
        case 1:
            EXPECT_WK_STREQ(url, "https://example.com/example");
            EXPECT_TRUE(frameInfo.isMainFrame);
            break;
        case 2:
            EXPECT_WK_STREQ(url, "https://webkit.org/webkit");
            EXPECT_FALSE(frameInfo.isMainFrame);
            break;
        default:
            EXPECT_FALSE(true);
            break;
        }
    }).get();
    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
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
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "parent frame received pingpong");

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

TEST(SiteIsolation, PostMessageWithMessagePorts)
{
    auto exampleHTML = "<script>"
    "    const channel = new MessageChannel();"
    "    channel.port1.onmessage = function() {"
    "        alert('parent frame received ' + event.data)"
    "    };"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*', [channel.port2]);"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "           event.ports[0].postMessage('got port and message ' + event.data);"
    "    }, false)"
    "</script>"_s;

    bool finishedLoading { false };
    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
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
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "parent frame received got port and message ping");

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

TEST(SiteIsolation, PostMessageWithNotAllowedTargetOrigin)
{
    auto exampleHTML = "<script>"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', 'https://foo.org');"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "    setTimeout(() => { alert('child did not receive message'); }, 1000);"
    "</script>"_s;

    bool finishedLoading { false };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse(exampleHTML).serialize());
                continue;
            }
            if (path == "/webkit"_s) {
                co_await connection.awaitableSend(HTTPResponse(webkitHTML).serialize());
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
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
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "child did not receive message");

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

TEST(SiteIsolation, PostMessageToIFrameWithOpaqueOrigin)
{
    auto exampleHTML = "<script>"
    "    onload = () => {"
    "        try {"
    "           document.getElementById('webkit_frame').contentWindow.postMessage('ping', 'data:');"
    "        } catch (error) {"
    "           alert(error);"
    "        }"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "</script>"_s;

    bool finishedLoading { false };
    
    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
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
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "SyntaxError: The string did not match the expected pattern.");

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

TEST(SiteIsolation, QueryFramesStateAfterNavigating)
{
    HTTPServer server({
        { "/page1.html"_s, { "<iframe src='subframe1.html'></iframe><iframe src='subframe2.html'></iframe><iframe src='subframe3.html'></iframe>"_s } },
        { "/page2.html"_s, { "<iframe src='subframe4.html'></iframe>"_s } },
        { "/subframe1.html"_s, { "SubFrame1"_s } },
        { "/subframe2.html"_s, { "SubFrame2"_s } },
        { "/subframe3.html"_s, { "SubFrame3"_s } },
        { "/subframe4.html"_s, { "SubFrame4"_s } }
    }, HTTPServer::Protocol::Http);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    [webView synchronouslyLoadRequest:server.request("/page1.html"_s)];
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(mainFrame.childFrames.count, 3U);
        done = true;
    }];
    Util::run(&done);

    [webView synchronouslyLoadRequest:server.request("/page2.html"_s)];
    done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(mainFrame.childFrames.count, 1U);
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, NavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
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
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame._processIdentifier;
        pid_t childFramePid = childFrame._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid); // FIXME: These should be equal.
        EXPECT_WK_STREQ(mainFrame.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.securityOrigin.host, "example.com");
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

TEST(SiteIsolation, IframeWithConfirm)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>confirm('confirm message')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForConfirm], "confirm message");

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

TEST(SiteIsolation, IframeWithPrompt)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>prompt('prompt message', 'default input')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForPromptWithReply:@"default input"], "prompt message");

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

TEST(SiteIsolation, GrandchildIframe)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<iframe srcdoc=\"<script>window.location='https://apple.com/apple'</script>\">"_s } },
        { "/apple"_s, { "<script>alert('grandchild loaded successfully')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "grandchild loaded successfully");

    // FIXME: Make the load event on the grandchild frame get called.
    // (add an onload in the response to /webkit and verify that it is actually called. It is not right now.)
}

// FIXME: This test times out on the iOS simulator. Investigate and fix.
#if PLATFORM(MAC)
TEST(SiteIsolation, MainFrameWithTwoIFramesInTheSameProcess)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame_1' src='https://webkit.org/a'></iframe><iframe id='webkit_frame_2' src='https://webkit.org/b'></iframe>"_s } },
        { "/a"_s, { "<script>alert('donea')</script>"_s } },
        { "/b"_s, { "<script>alert('doneb')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    // FIXME: call enableSiteIsolation to make this actually use site isolation
    // once we make changes so that this isn't flaky.

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    NSString* alert1 = [webView _test_waitForAlert];
    NSString* alert2 = [webView _test_waitForAlert];
    if ([alert1 isEqualToString:@"donea"])
        EXPECT_WK_STREQ(alert2, "doneb");
    else if ([alert1 isEqualToString:@"doneb"])
        EXPECT_WK_STREQ(alert2, "donea");
    else
        EXPECT_TRUE(false);

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(mainFrame.childFrames.count, 2u);
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        _WKFrameTreeNode *otherChildFrame = mainFrame.childFrames[1];
        pid_t mainFramePid = mainFrame._processIdentifier;
        pid_t childFramePid = childFrame._processIdentifier;
        pid_t otherChildFramePid = otherChildFrame._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(otherChildFramePid, 0);
        EXPECT_EQ(childFramePid, otherChildFramePid);
        // FIXME: Switch to EXPECT_NE once siteIsolation is enabled.
        EXPECT_EQ(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.securityOrigin.host, "webkit.org");
        EXPECT_WK_STREQ(otherChildFrame.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);
}
#endif

}
