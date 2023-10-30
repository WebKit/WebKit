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
#import "FrameTreeChecks.h"
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

static void enableWindowOpenPSON(WKWebViewConfiguration *configuration)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"ProcessSwapOnCrossSiteWindowOpenEnabled"]) {
            [preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

static std::pair<RetainPtr<WKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer& server)
{
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTFMove(webView), WTFMove(navigationDelegate) };
}

static bool processStillRunning(pid_t pid)
{
    return !kill(pid, 0);
}

static bool frameTreesMatch(_WKFrameTreeNode *actualRoot, const ExpectedFrameTree& expectedRoot)
{
    WKFrameInfo *info = actualRoot.info;
    if (info._isLocalFrame != std::holds_alternative<String>(expectedRoot.remoteOrOrigin))
        return false;

    if (auto* expectedOrigin = std::get_if<String>(&expectedRoot.remoteOrOrigin)) {
        WKSecurityOrigin *origin = info.securityOrigin;
        auto actualOrigin = makeString(String(origin.protocol), "://"_s, String(origin.host), origin.port ? makeString(':', origin.port) : String());
        if (actualOrigin != *expectedOrigin)
            return false;
    }

    if (actualRoot.childFrames.count != expectedRoot.children.size())
        return false;
    for (size_t i = 0; i < expectedRoot.children.size(); i++) {
        if (!frameTreesMatch(actualRoot.childFrames[i], expectedRoot.children[i]))
            return false;
    }
    return true;
}

static bool frameTreesMatch(NSSet<_WKFrameTreeNode *> *actualFrameTrees, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    if (actualFrameTrees.count != expectedFrameTrees.size())
        return false;

    for (_WKFrameTreeNode *root in actualFrameTrees) {
        auto index = expectedFrameTrees.findIf([&] (auto& expectedFrameTree) {
            return frameTreesMatch(root, expectedFrameTree);
        });
        if (index == WTF::notFound)
            return false;
        expectedFrameTrees.remove(index);
    }
    return expectedFrameTrees.isEmpty();
}

static RetainPtr<NSSet> frameTrees(WKWebView *webView)
{
    __block RetainPtr<NSSet> result;
    [webView _frameTrees:^(NSSet<_WKFrameTreeNode *> *frameTrees) {
        result = frameTrees;
    }];
    while (!result)
        Util::spinRunLoop();
    return result;
}

static void checkFrameTreesInProcesses(NSSet<_WKFrameTreeNode *> *actualTrees, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    EXPECT_TRUE(frameTreesMatch(actualTrees, WTFMove(expectedFrameTrees)));
}

void checkFrameTreesInProcesses(WKWebView *webView, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    checkFrameTreesInProcesses(frameTrees(webView).get(), WTFMove(expectedFrameTrees));
}

enum class FrameType : bool { Local, Remote };
static pid_t findFramePID(NSSet<_WKFrameTreeNode *> *set, FrameType local)
{
    for (_WKFrameTreeNode *node in set) {
        if (node.info._isLocalFrame == (local == FrameType::Local))
            return node.info._processIdentifier;
    }
    ASSERT_NOT_REACHED();
    return 0;
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
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

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

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "parent frame received pingpong");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, BasicPostMessageWindowOpen)
{
    auto exampleHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        w.postMessage('pong', '*');"
    "    }, false);"
    "</script>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('opened page received ' + event.data);"
    "    }, false);"
    "</script>"_s;

    __block bool openerFinishedLoading { false };
    __block bool openedFinishedLoading { false };
    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block RetainPtr<WKWebView> openerWebView;
    __block RetainPtr<WKWebView> openedWebView;

    auto openerNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavigationDelegate allowAnyTLSCertificate];
    openerNavigationDelegate.get().didFinishNavigation = ^(WKWebView *opener, WKNavigation *navigation) {
        EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
        checkFrameTreesInProcesses(opener, { { "https://example.com"_s } });
        openerFinishedLoading = true;
    };

    __block auto openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];
    openedNavigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://webkit.org/webkit");
        checkFrameTreesInProcesses(openerWebView.get(), { { "https://example.com"_s }, { RemoteFrame } });
        checkFrameTreesInProcesses(openedWebView.get(), { { "https://webkit.org"_s }, { RemoteFrame } });
        auto openerFrames = frameTrees(openerWebView.get());
        auto openedFrames = frameTrees(openedWebView.get());
        EXPECT_NE([openerWebView _webProcessIdentifier], [openedWebView _webProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Remote), [openedWebView _webProcessIdentifier]);
        EXPECT_EQ(findFramePID(openedFrames.get(), FrameType::Remote), [openerWebView _webProcessIdentifier]);
        openedFinishedLoading = true;
    };
    openedNavigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        auto openerFrames = frameTrees(openerWebView.get());
        checkFrameTreesInProcesses(openerFrames.get(), { { "https://example.com"_s }, { RemoteFrame } });
        checkFrameTreesInProcesses(openedWebView.get(), { { "https://example.com"_s } });
        EXPECT_EQ([openerWebView _webProcessIdentifier], [openedWebView _webProcessIdentifier]);
        EXPECT_NE([openedWebView _webProcessIdentifier], [openedWebView _provisionalWebProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Remote), [openedWebView _provisionalWebProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Local), [openerWebView _webProcessIdentifier]);
        completionHandler(WKNavigationResponsePolicyAllow);
    };

    auto configuration = server.httpsProxyConfiguration();
    enableWindowOpenPSON(configuration);

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().UIDelegate = uiDelegate.get();
        openedWebView.get().navigationDelegate = openedNavigationDelegate.get();
        return openedWebView.get();
    };

    openerWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    openerWebView.get().navigationDelegate = openerNavigationDelegate.get();
    openerWebView.get().UIDelegate = uiDelegate.get();
    openerWebView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&openerFinishedLoading);

    [openerWebView evaluateJavaScript:@"w = window.open('https://webkit.org/webkit')" completionHandler:nil];

    Util::run(&openedFinishedLoading);

    [openedWebView evaluateJavaScript:@"try { window.opener.postMessage('ping', '*'); } catch(e) { alert('error ' + e) }" completionHandler:nil];

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "opened page received pong");
}

struct WebViewAndDelegates {
    RetainPtr<WKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<TestUIDelegate> uiDelegate;
};

static std::pair<WebViewAndDelegates, WebViewAndDelegates> openerAndOpenedViews(const HTTPServer& server)
{
    __block WebViewAndDelegates opener;
    __block WebViewAndDelegates opened;
    opener.navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [opener.navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    enableWindowOpenPSON(configuration);
    opener.webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    opener.webView.get().navigationDelegate = opener.navigationDelegate.get();
    opener.uiDelegate = adoptNS([TestUIDelegate new]);
    opener.uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        enableSiteIsolation(configuration);
        enableWindowOpenPSON(configuration);
        opened.webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.navigationDelegate = adoptNS([TestNavigationDelegate new]);
        [opened.navigationDelegate allowAnyTLSCertificate];
        opened.uiDelegate = adoptNS([TestUIDelegate new]);
        opened.webView.get().navigationDelegate = opened.navigationDelegate.get();
        opened.webView.get().UIDelegate = opened.uiDelegate.get();
        return opened.webView.get();
    };
    [opener.webView setUIDelegate:opener.uiDelegate.get()];
    opener.webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [opener.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    while (!opened.webView)
        Util::spinRunLoop();
    [opened.navigationDelegate waitForDidFinishNavigation];
    return { WTFMove(opener), WTFMove(opened) };
}

TEST(SiteIsolation, NavigationAfterWindowOpen)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/example_opened_after_navigation"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });
    pid_t webKitPid = findFramePID(frameTrees(opener.webView.get()).get(), FrameType::Remote);

    [opened.webView evaluateJavaScript:@"window.location = 'https://example.com/example_opened_after_navigation'" completionHandler:nil];
    [opened.navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://example.com"_s } });

    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
}

TEST(SiteIsolation, CloseAfterWindowOpen)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    pid_t webKitPid = findFramePID(frameTrees(opener.webView.get()).get(), FrameType::Remote);
    [opener.webView evaluateJavaScript:@"w.close()" completionHandler:nil];
    [opened.uiDelegate waitForDidClose];
    opened.webView = nullptr;
    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
}

// FIXME: <rdar://117383420> Add a test that deallocates the opened WKWebView without being asked to by JS.
// Check state using native *and* JS APIs. Make sure processes are torn down as expected.
// Same with the opener WKWebView. We would probably need to set remotePageProxyInOpenerProcess
// to null manually to make the process terminate.
//
// Also test when the opener frame (if it's an iframe) is removed from the tree and garbage collected.
// That should probably do some teardown that should be visible from the API.

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
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "parent frame received got port and message ping");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
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
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "child did not receive message");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
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

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "SyntaxError: The string did not match the expected pattern.");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
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
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_EQ(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, ParentNavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://example.com/example_subframe' }</script>"_s } },
        { "/example_subframe"_s, { "<script>onload = ()=>{ alert('done') }</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_EQ(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
        done = true;
    }];
    Util::run(&done);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });
}

TEST(SiteIsolation, IframeNavigatesSelfWithoutChangingOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>window.location='/webkit_second'</script>"_s } },
        { "/webkit_second"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
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

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForConfirm], "confirm message");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
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

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForPromptWithReply:@"default input"], "prompt message");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, GrandchildIframe)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<iframe onload='alert(\"grandchild loaded successfully\")' srcdoc=\"<script>window.location='https://apple.com/apple'</script>\">"_s } },
        { "/apple"_s, { ""_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "grandchild loaded successfully");
}

TEST(SiteIsolation, GrandchildIframeSameOriginAsGrandparent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<iframe src='https://example.com/example_grandchild'></iframe>\">"_s } },
        { "/example_grandchild"_s, { "<script>alert('grandchild loaded successfully')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "grandchild loaded successfully");
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame, { { "https://example.com"_s } } } } },
        { RemoteFrame, { { "https://webkit.org"_s, { { RemoteFrame } } } } }
    });
}

TEST(SiteIsolation, ChildNavigatingToNewDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://foo.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "foo.com");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, ChildNavigatingToMainFrameDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://example.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_EQ(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, ChildNavigatingToSameDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://webkit.org/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, ChildNavigatingToDomainLoadedOnADifferentPage)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('done')</script>"_s } },
        { "/foo"_s, { "<iframe id='foo'><html><body><p>Hello world.</p></body></html></iframe>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [firstWebView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [firstWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/foo"]]];
    
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:firstWebView.get().configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block pid_t firstFramePID = 0;
    __block bool done { false };
    [firstWebView _frames:^(_WKFrameTreeNode *mainFrame) {
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        firstFramePID = mainFramePid;
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_NE(firstFramePID, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, MainFrameWithTwoIFramesInTheSameProcess)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame_1' src='https://webkit.org/a'></iframe><iframe id='webkit_frame_2' src='https://webkit.org/b'></iframe>"_s } },
        { "/a"_s, { "<script>alert('donea')</script>"_s } },
        { "/b"_s, { "<script>alert('doneb')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

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
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        pid_t otherChildFramePid = otherChildFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(otherChildFramePid, 0);
        EXPECT_EQ(childFramePid, otherChildFramePid);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
        EXPECT_WK_STREQ(otherChildFrame.info.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, ChildBeingNavigatedToMainFrameDomainByParent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done { false };
    __block pid_t childFramePid { 0 };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
        EXPECT_NE(childFramePid, mainFrame.info._processIdentifier);
        done = true;
    }];
    Util::run(&done);

    [webView evaluateJavaScript:@"document.getElementById('webkit_frame').src = 'https://example.com/example_subframe'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });

    while (processStillRunning(childFramePid))
        Util::spinRunLoop();
}

TEST(SiteIsolation, ChildBeingNavigatedToSameDomainByParent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://webkit.org/example_subframe' }</script>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
        pid_t mainFramePid = mainFrame.info._processIdentifier;
        pid_t childFramePid = childFrame.info._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_NE(childFramePid, 0);
        EXPECT_NE(mainFramePid, childFramePid);
        EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
        EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
        done = true;
    }];
    Util::run(&done);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, ChildBeingNavigatedToNewDomainByParent)
{
    auto appleHTML = "<script>"
        "window.addEventListener('message', (event) => {"
        "    parent.window.postMessage(event.data + 'pong', { 'targetOrigin' : '*' });"
        "}, false);"
        "alert('apple iframe loaded')"
        "</script>"_s;

    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://apple.com/apple' }</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } },
        { "/apple"_s, { appleHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "apple iframe loaded");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://apple.com"_s } }
        }
    });

    NSString *jsCheckingPostMessageRoundTripAfterIframeProcessChange = @""
    "window.addEventListener('message', (event) => {"
    "    alert('parent frame received ' + event.data)"
    "}, false);"
    "document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*');";
    [webView evaluateJavaScript:jsCheckingPostMessageRoundTripAfterIframeProcessChange completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "parent frame received pingpong");
}

TEST(SiteIsolation, IframeRedirectSameSite)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { 302, { { "Location"_s, "https://www.webkit.org/www_webkit"_s } }, "redirecting..."_s } },
        { "/www_webkit"_s, { "arrived!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://www.webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, IframeRedirectCrossSite)
{
    HTTPServer server({
        { "/example1"_s, { "<iframe src='https://webkit.org/webkit1'></iframe>"_s } },
        { "/webkit1"_s, { 302, { { "Location"_s, "https://apple.com/apple1"_s } }, "redirecting..."_s } },
        { "/apple1"_s, { "arrived!"_s } },
        { "/example2"_s, { "<iframe src='https://webkit.org/webkit2'></iframe>"_s } },
        { "/webkit2"_s, { 302, { { "Location"_s, "https://webkit.org/webkit3"_s } }, "redirecting..."_s } },
        { "/webkit3"_s, { 302, { { "Location"_s, "https://example.com/example3"_s } }, "redirecting..."_s } },
        { "/example3"_s, { "arrived!"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://apple.com"_s } }
        }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example2"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });
}

TEST(SiteIsolation, NavigationWithIFrames)
{
    HTTPServer server({
        { "/1"_s, { "<iframe src='https://domain2.com/2'></iframe>"_s } },
        { "/2"_s, { "hi!"_s } },
        { "/3"_s, { "<iframe src='https://domain4.com/4'></iframe>"_s } },
        { "/4"_s, { "<iframe src='https://domain5.com/5'></iframe>"_s } },
        { "/5"_s, { "hi!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/1"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/3"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    // FIXME: Implement CachedFrame for RemoteFrames and verify the page is resumed correctly.
}

TEST(SiteIsolation, RemoveFrames)
{
    HTTPServer server({
        { "/webkit_main"_s, { "<iframe src='https://webkit.org/webkit_iframe' id='wk'></iframe><iframe src='https://example.com/example_iframe' id='ex'></iframe>"_s } },
        { "/webkit_iframe"_s, { "hi!"_s } },
        { "/example_iframe"_s, { "<iframe src='example_grandchild_frame'></iframe>"_s } },
        { "/example_grandchild_frame"_s, { "hi!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit_main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { "https://webkit.org"_s }, { RemoteFrame, { { RemoteFrame } } } }
        }, { RemoteFrame,
            { { RemoteFrame }, { "https://example.com"_s, { { "https://example.com"_s } } } }
        }
    });

    __block bool removedLocalFrame { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('wk');frame.parentNode.removeChild(frame);" completionHandler:^(id, NSError *error) {
        removedLocalFrame = true;
    }];
    Util::run(&removedLocalFrame);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame, { { RemoteFrame } } } }
        }, { RemoteFrame,
            { { "https://example.com"_s, { { "https://example.com"_s } } } }
        }
    });

    __block bool removedRemoteFrame { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('ex');frame.parentNode.removeChild(frame);" completionHandler:^(id, NSError *error) {
        removedRemoteFrame = true;
    }];
    Util::run(&removedRemoteFrame);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s }
    });
}

TEST(SiteIsolation, ProvisionalLoadFailure)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { HTTPResponse::TerminateConnection::Yes } },
        { "/apple"_s,  { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { "https://example.com"_s } } }
    });

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');document.body.appendChild(iframe);iframe.src = 'https://apple.com/apple'" completionHandler:nil];
    Vector<ExpectedFrameTree> expectedFrameTreesAfterAddingApple { {
        "https://example.com"_s, { { "https://example.com"_s }, { RemoteFrame } }
    }, {
        RemoteFrame, { { RemoteFrame }, { "https://apple.com"_s } }
    } };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedFrameTreesAfterAddingApple }))
        Util::spinRunLoop();

    [webView evaluateJavaScript:@"iframe.onload = alert('done');iframe.src = 'https://webkit.org/webkit'" completionHandler:nil];

    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");
    checkFrameTreesInProcesses(webView.get(), WTFMove(expectedFrameTreesAfterAddingApple));
}

TEST(SiteIsolation, MultipleReloads)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s,  { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });

    [webView reload];
    Util::runFor(0.1_s);
    [webView reload];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
}

#if PLATFORM(MAC)
TEST(SiteIsolation, PropagateMouseEventsToSubframe)
{
    auto mainframeHTML = "<script>"
    "    window.eventTypes = [];"
    "    window.addEventListener('message', function(event) {"
    "        window.eventTypes.push(event.data);"
    "    });"
    "</script>"
    "<iframe src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<script>"
    "    addEventListener('mousemove', window.parent.postMessage('mousemove', '*'));"
    "    addEventListener('mousedown', (event) => { window.parent.postMessage('mousedown,' + event.pageX + ',' + event.pageY, '*') });"
    "    addEventListener('mouseup', (event) => { window.parent.postMessage('mouseup,' + event.pageX + ',' + event.pageY, '*') });"
    "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(50, 50) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    NSArray<NSString *> *eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    EXPECT_EQ(3U, eventTypes.count);
    EXPECT_WK_STREQ("mousemove", eventTypes[0]);
    EXPECT_WK_STREQ("mousedown,40,40", eventTypes[1]);
    EXPECT_WK_STREQ("mouseup,40,40", eventTypes[2]);
}
#endif

TEST(SiteIsolation, ShutDownFrameProcessesAfterNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hello"_s } },
        { "/apple"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), { { "https://apple.com"_s } });

    while (processStillRunning(iframePID))
        Util::spinRunLoop();
}

TEST(SiteIsolation, OpenerProcessSharing)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/opener_iframe'></iframe>"_s } },
        { "/opened"_s, { "<iframe src='https://webkit.org/opened_iframe'></iframe>"_s } },
        { "/opener_iframe"_s, { "hello"_s } },
        { "/opened_iframe"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server);

    __block RetainPtr<WKWebView> openedWebView;
    __block auto uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        static auto openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
        [openedNavigationDelegate allowAnyTLSCertificate];
        openedWebView.get().navigationDelegate = openedNavigationDelegate.get();
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"w = window.open('/opened')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "done");

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *openerMainFrame) {
        __block RetainPtr<_WKFrameTreeNode> retainedOpenerMainFrame = openerMainFrame;
        [openedWebView _frames:^(_WKFrameTreeNode *openedMainFrame) {
            pid_t openerMainFramePid = retainedOpenerMainFrame.get().info._processIdentifier;
            pid_t openedMainFramePid = openedMainFrame.info._processIdentifier;
            pid_t openerIframePid = retainedOpenerMainFrame.get().childFrames.firstObject.info._processIdentifier;
            pid_t openedIframePid = openedMainFrame.childFrames.firstObject.info._processIdentifier;

            EXPECT_EQ(openerMainFramePid, openedMainFramePid);
            EXPECT_NE(openerMainFramePid, 0);
            EXPECT_EQ(openerIframePid, openedIframePid);
            EXPECT_NE(openerIframePid, 0);
            done = true;
        }];
    }];
    Util::run(&done);
}

TEST(SiteIsolation, SetFocusedFrame)
{
    auto mainframeHTML = "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr<WKFrameInfo> mainFrameInfo;
    RetainPtr<WKFrameInfo> childFrameInfo;
    auto getUpdatedFrameInfo = [&] {
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            mainFrameInfo = mainFrame.info;
            childFrameInfo = mainFrame.childFrames.firstObject.info;
            done = true;
        }];
        Util::run(&done);
    };

    getUpdatedFrameInfo();
    EXPECT_FALSE(mainFrameInfo.get()._isFocused);
    EXPECT_FALSE(childFrameInfo.get()._isFocused);

    done = false;
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:^(id value, NSError *error) {
        done = true;
    }];
    Util::run(&done);

    do {
        getUpdatedFrameInfo();
    } while (mainFrameInfo.get()._isFocused || !childFrameInfo.get()._isFocused);

    done = false;
    [webView evaluateJavaScript:@"window.focus()" completionHandler:^(id value, NSError *error) {
        done = true;
    }];
    Util::run(&done);

    do {
        getUpdatedFrameInfo();
    } while (!mainFrameInfo.get()._isFocused || childFrameInfo.get()._isFocused);
}

TEST(SiteIsolation, EvaluateJavaScriptInFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<script>test = 'abc';</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<WKFrameInfo> childFrameInfo;
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFrameInfo = mainFrame.childFrames.firstObject.info;
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView evaluateJavaScript:@"window.test" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
        EXPECT_STREQ([result UTF8String], "abc");
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, MainFrameURLAfterFragmentNavigation)
{
    NSString *json = @"["
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"blocked_when_fragment_in_top_url\", \"if-top-url\":[\"fragment\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"always_blocked\", \"if-top-url\":[\"http\"]}}"
    "]";
    __block bool doneRemoving { false };
    [WKContentRuleListStore.defaultStore removeContentRuleListForIdentifier:@"Identifier" completionHandler:^(NSError *error) {
        doneRemoving = true;
    }];
    Util::run(&doneRemoving);
    __block RetainPtr<WKContentRuleList> list;
    [WKContentRuleListStore.defaultStore compileContentRuleListForIdentifier:@"Identifier" encodedContentRuleList:json completionHandler:^(WKContentRuleList *ruleList, NSError *error) {
        list = ruleList;
    }];
    while (!list)
        Util::spinRunLoop();

    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/blocked_when_fragment_in_top_url"_s, { "loaded successfully"_s } },
        { "/always_blocked"_s, { "loaded successfully"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView.get().configuration.userContentController addContentRuleList:list.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<WKFrameInfo> childFrameInfo;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFrameInfo = mainFrame.childFrames.firstObject.info;
    }];
    while (!childFrameInfo)
        Util::spinRunLoop();

    auto canLoadURLInIFrame = [childFrameInfo = RetainPtr { childFrameInfo }, webView = RetainPtr { webView }] (NSString *path) -> bool {
        __block std::optional<bool> loadedSuccessfully;
        [webView callAsyncJavaScript:[NSString stringWithFormat:@"try { let response = await fetch('%@'); return await response.text() } catch (e) { return 'load failed' }", path] arguments:nil inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
            if ([result isEqualToString:@"loaded successfully"])
                loadedSuccessfully = true;
            else if ([result isEqualToString:@"load failed"])
                loadedSuccessfully = false;
            else
                EXPECT_FALSE(true);
        }];
        while (!loadedSuccessfully)
            Util::spinRunLoop();
        return *loadedSuccessfully;
    };
    EXPECT_TRUE(canLoadURLInIFrame(@"/blocked_when_fragment_in_top_url"));
    EXPECT_FALSE(canLoadURLInIFrame(@"/always_blocked"));

    [webView evaluateJavaScript:@"window.location = '#fragment'" completionHandler:nil];
    while (![webView.get().URL.fragment isEqualToString:@"fragment"])
        Util::spinRunLoop();

    EXPECT_FALSE(canLoadURLInIFrame(@"/blocked_when_fragment_in_top_url"));
    EXPECT_FALSE(canLoadURLInIFrame(@"/always_blocked"));
}

}
