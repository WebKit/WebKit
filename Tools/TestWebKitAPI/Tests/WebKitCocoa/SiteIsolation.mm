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
#import "DragAndDropSimulator.h"
#import "FrameTreeChecks.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewFindStringFindDelegate.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
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

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer& server)
{
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
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

static Vector<char> indentation(size_t count)
{
    Vector<char> result;
    for (size_t i = 0; i < count; i++)
        result.append(' ');
    result.append(0);
    return result;
}

static void printTree(_WKFrameTreeNode *n, size_t indent = 0)
{
    if (n.info._isLocalFrame)
        WTFLogAlways("%s%@://%@ (pid %d)", indentation(indent).data(), n.info.securityOrigin.protocol, n.info.securityOrigin.host, n.info._processIdentifier);
    else
        WTFLogAlways("%s(remote) (pid %d)", indentation(indent).data(), n.info._processIdentifier);
    for (_WKFrameTreeNode *c in n.childFrames)
        printTree(c, indent + 1);
}

static void printTree(const ExpectedFrameTree& n, size_t indent = 0)
{
    if (auto* s = std::get_if<String>(&n.remoteOrOrigin))
        WTFLogAlways("%s%s", indentation(indent).data(), s->utf8().data());
    else
        WTFLogAlways("%s(remote)", indentation(indent).data());
    for (const auto& c : n.children)
        printTree(c, indent + 1);
}

static void checkFrameTreesInProcesses(NSSet<_WKFrameTreeNode *> *actualTrees, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    bool result = frameTreesMatch(actualTrees, WTFMove(expectedFrameTrees));
    if (!result) {
        WTFLogAlways("ACTUAL");
        for (_WKFrameTreeNode *n in actualTrees)
            printTree(n);
        WTFLogAlways("EXPECTED");
        for (const auto& e : expectedFrameTrees)
            printTree(e);
        WTFLogAlways("END");
    }
    EXPECT_TRUE(result);
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
    EXPECT_FALSE(true);
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
    enableSiteIsolation(configuration);

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

static std::pair<WebViewAndDelegates, WebViewAndDelegates> openerAndOpenedViews(const HTTPServer& server, NSString *url = @"https://example.com/example", bool waitForOpenedNavigation = true)
{
    __block WebViewAndDelegates opener;
    __block WebViewAndDelegates opened;
    opener.navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [opener.navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    opener.webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    opener.webView.get().navigationDelegate = opener.navigationDelegate.get();
    opener.uiDelegate = adoptNS([TestUIDelegate new]);
    opener.uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        enableSiteIsolation(configuration);
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
    [opener.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    while (!opened.webView)
        Util::spinRunLoop();
    if (waitForOpenedNavigation)
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

TEST(SiteIsolation, PreferencesUpdatesToAllProcesses)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } },
        { "/opened"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = NO;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<WKFrameInfo> childFrame;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFrame = mainFrame.childFrames[0].info;
    }];
    while (!childFrame)
        Util::spinRunLoop();

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    __block bool opened { false };
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures)
    {
        opened = true;
        return nil;
    };
    [webView setUIDelegate:uiDelegate.get()];

    [webView _evaluateJavaScript:@"window.open('https://example.com/opened')" withSourceURL:nil inFrame:childFrame.get() inContentWorld:WKContentWorld.pageWorld withUserGesture:NO completionHandler:nil];
    Util::run(&opened);
}

TEST(SiteIsolation, ParentOpener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);

    __block RetainPtr<WKFrameInfo> childFrame;
    [opened.webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFrame = mainFrame.childFrames[0].info;
    }];
    while (!childFrame)
        Util::spinRunLoop();

    [opened.webView evaluateJavaScript:@"try { opener.postMessage('test1', '*'); alert('posted message 1') } catch(e) { alert(e) }" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "posted message 1");

    [opened.webView evaluateJavaScript:@"try { top.opener.postMessage('test2', '*'); alert('posted message 2') } catch(e) { alert(e) }" inFrame:childFrame.get() inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "posted message 2");
}

TEST(SiteIsolation, WindowOpenRedirect)
{
    HTTPServer server({
        { "/example1"_s, { "<script>w = window.open('https://webkit.org/webkit1')</script>"_s } },
        { "/webkit1"_s, { 302, { { "Location"_s, "/webkit2"_s } }, "redirecting..."_s } },
        { "/webkit2"_s, { "loaded!"_s } },
        { "/example2"_s, { "<script>w = window.open('https://webkit.org/webkit3')</script>"_s } },
        { "/webkit3"_s, { 302, { { "Location"_s, "https://example.com/example3"_s } }, "redirecting..."_s } },
        { "/example3"_s, { "loaded!"_s } },
        { "/example4"_s, { "<script>w = window.open('https://webkit.org/webkit4')</script>"_s } },
        { "/webkit4"_s, { 302, { { "Location"_s, "https://apple.com/apple"_s } }, "redirecting..."_s } },
        { "/apple"_s, { "loaded!"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example1");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://webkit.org/webkit2");
    }
    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example2");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://example.com/example3");
    }
    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example4");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://apple.com/apple");
    }
}

TEST(SiteIsolation, CloseAfterWindowOpen)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    pid_t webKitPid = findFramePID(frameTrees(opener.webView.get()).get(), FrameType::Remote);

    __block bool done = false;
    [opener.webView evaluateJavaScript:@"w.closed" completionHandler:^(id result, NSError *) {
        EXPECT_FALSE([result boolValue]);
        done = true;
    }];
    Util::run(&done);

    [opener.webView evaluateJavaScript:@"w.close()" completionHandler:nil];
    [opened.uiDelegate waitForDidClose];
    opened.webView = nullptr;
    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });

    done = false;
    [opener.webView evaluateJavaScript:@"w.closed" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    Util::run(&done);
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
        EXPECT_EQ(firstFramePID, childFramePid);
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

TEST(SiteIsolation, CrossOriginOpenerPolicy)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "iframe content"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
    __block bool done { false };
    [webView _doAfterNextPresentationUpdate:^{
        done = true;
    }];
    Util::run(&done);
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
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/3"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain3.com"_s, { { RemoteFrame, { { RemoteFrame } } } } },
        { RemoteFrame, { { "https://domain4.com"_s, { { RemoteFrame } } } } },
        { RemoteFrame, { { RemoteFrame, { { "https://domain5.com"_s } } } } }
    });

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    });
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
        { "/webkit"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } },
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
    "    addEventListener('mousemove', (event) => { window.parent.postMessage('mousemove', '*') });"
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
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    NSArray<NSString *> *eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    EXPECT_EQ(3U, eventTypes.count);
    EXPECT_WK_STREQ("mousemove", eventTypes[0]);
    EXPECT_WK_STREQ("mousedown,40,40", eventTypes[1]);
    EXPECT_WK_STREQ("mouseup,40,40", eventTypes[2]);
}

TEST(SiteIsolation, DragEvents)
{
    auto mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue;'></div>"
    "<script>"
    "    draggable.addEventListener('dragstart', (event) => { window.parent.postMessage('dragstart', '*') });"
    "    draggable.addEventListener('dragend', (event) => { window.parent.postMessage('dragend', '*') });"
    "    draggable.addEventListener('dragenter', (event) => { window.parent.postMessage('dragenter', '*') });"
    "    draggable.addEventListener('dragleave', (event) => { window.parent.postMessage('dragleave', '*') });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);
    RetainPtr webView = [simulator webView];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 150)];

    NSArray<NSString *> *events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_EQ(4U, events.count);
    EXPECT_WK_STREQ("dragstart", events[0]);
    EXPECT_WK_STREQ("dragenter", events[1]);
    EXPECT_WK_STREQ("dragleave", events[2]);
    EXPECT_WK_STREQ("dragend", events[3]);
}

void writeImageDataToPasteboard(NSString *type, NSData *data)
{
    [NSPasteboard.generalPasteboard declareTypes:@[type] owner:nil];
    [NSPasteboard.generalPasteboard setData:data forType:type];
}

TEST(SiteIsolation, PasteGIF)
{
    auto mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div id='editor' contenteditable style=\"height:100%; width: 100%;\"></div>"
    "<script>"
    "const editor = document.getElementById('editor');"
    "editor.focus();"
    "editor.addEventListener('paste', (event) => { window.parent.postMessage(event.clipboardData.files[0].name, '*'); });"
    "</script>"
    "</body>"_s;

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

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(20, 20) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    auto *data = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-400px" ofType:@"gif" inDirectory:@"TestWebKitAPI.resources"]];
    writeImageDataToPasteboard((__bridge NSString *)kUTTypeGIF, data);
    [webView paste:nil];

    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    NSArray<NSString *> *events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_EQ(1U, events.count);
    EXPECT_WK_STREQ("image.gif", events[0]);
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

    __block bool done = false;
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

TEST(SiteIsolation, FocusOpenedWindow)
{
    auto openerHTML = "<script>"
    "    let w = window.open('https://domain2.com/opened');"
    "</script>"_s;
    HTTPServer server({
        { "/example"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);

    RetainPtr<WKFrameInfo> openerInfo;
    RetainPtr<WKFrameInfo> openedInfo;
    auto getUpdatedFrameInfo = [&] (WKWebView *openerWebView, WKWebView *openedWebView) {
        __block bool done = false;
        [openerWebView _frames:^(_WKFrameTreeNode *mainFrame) {
            openerInfo = mainFrame.info;
            done = true;
        }];
        Util::run(&done);

        done = false;
        [openedWebView _frames:^(_WKFrameTreeNode *mainFrame) {
            openedInfo = mainFrame.info;
            done = true;
        }];
        Util::run(&done);
    };

    getUpdatedFrameInfo(opener.webView.get(), opened.webView.get());
    EXPECT_FALSE([openerInfo _isFocused]);
    EXPECT_FALSE([openedInfo _isFocused]);

    [opener.webView.get() evaluateJavaScript:@"w.focus()" completionHandler:nil];

    do {
        getUpdatedFrameInfo(opener.webView.get(), opened.webView.get());
    } while (![openedInfo _isFocused]);
    EXPECT_FALSE([openerInfo _isFocused]);
}

#if PLATFORM(MAC)
TEST(SiteIsolation, OpenedWindowFocusDelegates)
{
    auto openerHTML = "<script>"
        "    let w = window.open('https://domain2.com/opened');"
        "</script>"_s;
    HTTPServer server({
        { "/example"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);

    __block bool calledFocusWebView = false;
    [opened.uiDelegate setFocusWebView:^(WKWebView *viewToFocus) {
        calledFocusWebView = true;
    }];

    __block bool calledUnfocusWebView = false;
    [opened.uiDelegate setUnfocusWebView:^(WKWebView *viewToFocus) {
        calledUnfocusWebView = true;
    }];

    [opener.webView.get() evaluateJavaScript:@"w.focus()" completionHandler:nil];
    Util::run(&calledFocusWebView);

    [opener.webView.get() evaluateJavaScript:@"w.blur()" completionHandler:nil];
    Util::run(&calledUnfocusWebView);
}
#endif

TEST(SiteIsolation, FindStringInFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);

    __block bool done = false;
    [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"Missing string" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(SiteIsolation, FindStringInNestedFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nested_subframe'></iframe>"_s } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);

    __block bool done = false;
    [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"Missing string" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(SiteIsolation, FindStringSelection)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(2UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrameNode.get().childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionWithEmptyFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/empty_subframe'></iframe>"
        "<iframe src='https://domain4.com/subframe'></iframe>"
        "<iframe src='https://domain5.com/empty_subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } },
        { "/empty_subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(4UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrameNode.get().childFrames[2].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionNoWrap)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    findConfiguration.get().wraps = NO;
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(1UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
    };

    std::array<SelectionOffsets, 3> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionBackwards)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    findConfiguration.get().backwards = YES;
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(2UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrameNode.get().childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
}

TEST(SiteIsolation, FindStringSelectionSameOriginFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(2UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrameNode.get().childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionNestedFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    auto subframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain4.com/nested_subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 5>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(2UL, mainFrame.childFrames.count);
            EXPECT_EQ(1UL, mainFrame.childFrames[0].childFrames.count);
            EXPECT_EQ(1UL, mainFrame.childFrames[1].childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrameNode.get().childFrames[1].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[3].first endOffset:offsets[3].second inFrame:mainFrameNode.get().childFrames[0].childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[4].first endOffset:offsets[4].second inFrame:mainFrameNode.get().childFrames[1].childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 11 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionMultipleMatchesInMainFrame)
{
    auto mainframeHTML = "<p>Hello world Hello world Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(1UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 12, 23 }, { 0, 0 } } },
        { { { 24, 35 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionMultipleMatchesInChildFrame)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world Hello world Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(1UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 12, 23 } } },
        { { { 0, 0 }, { 24, 35 } } },
        { { { 0, 11 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionSameOriginFrameBeforeWrap)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // FIXME(267907): If the iframe is not added like this the UI process and web processes may have mismatched frame trees.
    auto addFrameToBody = @"let frame = document.createElement('iframe');"
    "frame.setAttribute('src', 'https://domain1.com/subframe');"
    "document.body.appendChild(frame);";
    __block bool done = false;
    [webView evaluateJavaScript:addFrameToBody completionHandler:^(id _Nullable, NSError * _Nullable error) {
        done = true;
    }];
    Util::run(&done);

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        __block RetainPtr<_WKFrameTreeNode> mainFrameNode;
        __block bool done = false;
        [webView _frames:^(_WKFrameTreeNode *mainFrame) {
            EXPECT_EQ(2UL, mainFrame.childFrames.count);
            mainFrameNode = mainFrame;
            done = true;
        }];
        Util::run(&done);
        done = false;

        [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
            EXPECT_TRUE(result.matchFound);
            done = true;
        }];
        Util::run(&done);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrameNode.get().info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrameNode.get().childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrameNode.get().childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringMatchCount)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    auto findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [webView findString:@"Hello world" withConfiguration:findConfiguration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        done = true;
    }];
    Util::run(&done);

    EXPECT_EQ(3ul, [findDelegate matchesCount]);
}

#if PLATFORM(MAC)
TEST(SiteIsolation, ProcessDisplayNames)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "<script></script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    __block bool done { false };
    [webView.get().configuration.websiteDataStore removeDataOfTypes:WKWebsiteDataStore.allWebsiteDataTypes modifiedSince:NSDate.distantPast completionHandler:^{
        done = true;
    }];
    Util::run(&done);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t mainFramePID { 0 };
    pid_t iframePID { 0 };
    auto trees = frameTrees(webView.get());
    EXPECT_EQ([trees count], 2u);
    for (_WKFrameTreeNode *tree in trees.get()) {
        if (tree.info._isLocalFrame)
            mainFramePID = tree.info._processIdentifier;
        else if (tree.childFrames.count)
            iframePID = tree.childFrames[0].info._processIdentifier;
    }
    EXPECT_NE(mainFramePID, iframePID);
    EXPECT_NE(mainFramePID, 0);
    EXPECT_NE(iframePID, 0);

    done = false;
    WKProcessPool *pool = webView.get().configuration.processPool;
    [pool _getActivePagesOriginsInWebProcessForTesting:mainFramePID completionHandler:^(NSArray<NSString *> *result) {
        EXPECT_EQ(result.count, 1u);
        EXPECT_WK_STREQ(result[0], "https://example.com");
        done = true;
    }];
    Util::run(&done);

    done = false;
    [pool _getActivePagesOriginsInWebProcessForTesting:iframePID completionHandler:^(NSArray<NSString *> *result) {
        EXPECT_EQ(result.count, 1u);
        EXPECT_WK_STREQ(result[0], "https://apple.com");
        done = true;
    }];
    Util::run(&done);
}
#endif

TEST(SiteIsolation, NavigateOpener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/webkit2"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    [opened.webView evaluateJavaScript:@"opener.location = '/webkit2'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://webkit.org"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://webkit.org"_s } });
}

TEST(SiteIsolation, NavigateOpenerToProvisionalNavigationFailure)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://webkit.org/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://example.com/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://apple.com/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });
}

TEST(SiteIsolation, NavigateIframeToProvisionalNavigationFailure)
{
    HTTPServer server({
        { "/webkit"_s, { "<iframe id='testiframe' src='https://example.com/example'></iframe>"_s } },
        { "/example"_s, { "hi"_s } },
        { "/redirect_to_example_terminate"_s, { 302, { { "Location"_s, "https://example.com/terminate"_s } }, "redirecting..."_s } },
        { "/redirect_to_webkit_terminate"_s, { 302, { { "Location"_s, "https://webkit.org/terminate"_s } }, "redirecting..."_s } },
        { "/redirect_to_apple_terminate"_s, { 302, { { "Location"_s, "https://apple.com/terminate"_s } }, "redirecting..."_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s } }
        },
    });

    __block bool provisionalLoadFailed { false };
    navigationDelegate.get().didFailProvisionalLoadWithRequestInFrameWithError = ^(WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo, NSError *error) {
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        EXPECT_EQ(error.code, NSURLErrorNetworkConnectionLost);
        EXPECT_FALSE(frameInfo.isMainFrame);
        provisionalLoadFailed = true;
    };

    __block RetainPtr blockScopeWebView { webView };
    auto checkProvisionalLoadFailure = ^(NSString *url) {
        provisionalLoadFailed = false;
        [blockScopeWebView evaluateJavaScript:[NSString stringWithFormat:@"document.getElementById('testiframe').src = '%@'", url] completionHandler:nil];
        while (!provisionalLoadFailed)
            Util::spinRunLoop();
        checkFrameTreesInProcesses(blockScopeWebView.get(), {
            { "https://webkit.org"_s,
                { { RemoteFrame } }
            }, { RemoteFrame,
                { { "https://example.com"_s } }
            },
        });
    };
    checkProvisionalLoadFailure(@"https://example.com/terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/terminate");
    checkProvisionalLoadFailure(@"https://apple.com/terminate");

    // FIXME: Add tests navigating the iframe to each redirect_to_*_terminate.
}

// FIXME: If a provisional load happens in a RemoteFrame with frame children, does anything clear out those
// child frames when the load commits? Probably not. Needs a test.

// FIXME: If we cancel a provisional load before it commits, we need another message to destroy this provisional frame.

// FIXME: Add a test that verifies that provisional frames are not accessible via DOMWindow.frames.

// FIXME: Make a test that tries to access its parent that used to be remote during a provisional navigation of
// the parent to that domain to verify that even the main frame uses provisional frames.

TEST(SiteIsolation, OpenThenClose)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<WKWebView> retainOpener;
    @autoreleasepool {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
        retainOpener = opener.webView;
    }
}

TEST(SiteIsolation, CustomUserAgent)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<WKFrameInfo> childFrameInfo;
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(1UL, mainFrame.childFrames.count);
        childFrameInfo = mainFrame.childFrames.firstObject.info;
        done = true;
    }];
    Util::run(&done);
    done = false;

    webView.get().customUserAgent = @"Custom UserAgent";

    [webView evaluateJavaScript:@"navigator.userAgent" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *) {
        EXPECT_WK_STREQ(@"Custom UserAgent", (NSString *)result);
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, ApplicationNameForUserAgent)
{
    auto mainframeHTML = "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain3.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex].endsWith(" Custom UserAgent"_s));
                receivedRequestFromSubframe = true;
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView _setApplicationNameForUserAgent:@"Custom UserAgent"];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<WKFrameInfo> childFrameInfo;
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(1UL, mainFrame.childFrames.count);
        childFrameInfo = mainFrame.childFrames.firstObject.info;
        done = true;
    }];
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([result hasSuffix:@" Custom UserAgent"]);
        done = true;
    }];
    Util::run(&done);
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgent)
{
    auto mainframeHTML = "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain3.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    bool firstRequest = true;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                if (firstRequest)
                    EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                else
                    EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent2"_s);
                receivedRequestFromSubframe = true;
                firstRequest = false;
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    __block RetainPtr<WKFrameInfo> childFrameInfo;
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(1UL, mainFrame.childFrames.count);
        childFrameInfo = mainFrame.childFrames.firstObject.info;
        done = true;
    }];
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *) {
        EXPECT_WK_STREQ(@"Custom UserAgent", (NSString *)result);
        done = true;
    }];
    Util::run(&done);
    done = false;

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    Util::run(&receivedRequestFromSubframe);

    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        EXPECT_EQ(1UL, mainFrame.childFrames.count);
        childFrameInfo = mainFrame.childFrames.firstObject.info;
        done = true;
    }];
    Util::run(&done);
    done = false;

    [webView evaluateJavaScript:@"navigator.userAgent" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *) {
        EXPECT_WK_STREQ(@"Custom UserAgent2", (NSString *)result);
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgentDuringCrossSiteProvisionalNavigation)
{
    auto mainframeHTML = "<iframe id='frame' src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain2.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                receivedRequestFromSubframe = true;
                continue;
            }
            if (path == "/missing"_s)
                continue;
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *webView, WKNavigation *) {
        [webView evaluateJavaScript:@"document.getElementById('frame').src = 'https://domain4.com/subframe';" completionHandler:nil];
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/missing"]]];
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, LoadHTMLString)
{
    HTTPServer server({
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    NSString *html = @"<iframe src='https://webkit.org/webkit'></iframe>";
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.com"]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });

    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgentDuringSameSiteProvisionalNavigation)
{
    auto mainframeHTML = "<iframe id='frame' src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain2.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                receivedRequestFromSubframe = true;
                continue;
            }
            if (path == "/missing"_s)
                continue;
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *webView, WKNavigation *) {
        [webView evaluateJavaScript:@"document.getElementById('frame').src = 'https://domain3.com/subframe';" completionHandler:nil];
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/missing"]]];
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, ProvisionalLoadFailureOnCrossSiteRedirect)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { ""_s } },
        { "/redirect"_s, { 302, { { "Location"_s, "https://example.com/terminate"_s } }, "redirecting..."_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<WKFrameInfo> childFrameInfo;
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFrameInfo = mainFrame.childFrames.firstObject.info;
        done = true;
    }];
    Util::run(&done);

    done = false;
    navigationDelegate.get().didFailProvisionalLoadWithRequestInFrameWithError = ^(WKWebView *, NSURLRequest *request, WKFrameInfo *, NSError *) {
        EXPECT_WK_STREQ(request.URL.absoluteString, "https://example.com/terminate");
        done = true;
    };
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/redirect'" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    Util::run(&done);
}

#if PLATFORM(MAC)
TEST(SiteIsolation, SelectAll)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe' src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "test"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:^(id, NSError *) {
        done = true;
    }];
    Util::run(&done);
    done = false;

    __block RetainPtr<_WKFrameTreeNode> childFrameNode;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFrameNode = mainFrame.childFrames[0];
        done = true;
    }];
    Util::run(&done);

    [webView selectAll:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrameNode.get().info])
        Util::spinRunLoop();
}

TEST(SiteIsolation, TopContentInsetAfterCrossSiteNavigation)
{
    HTTPServer server({
        { "/source"_s, { "<script> location.href = 'https://webkit.org/destination'; </script>"_s } },
        { "/destination"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _setTopContentInset:10];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/source"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [webView evaluateJavaScript:@"window.innerHeight" completionHandler:^(id result, NSError *) {
        EXPECT_EQ(-10, [result intValue]);
        done = true;
    }];
    Util::run(&done);
}
#endif

TEST(SiteIsolation, PresentationUpdateAfterCrossSiteNavigation)
{
    HTTPServer server({
        { "/source"_s, { "<script> location.href = 'https://webkit.org/destination'; </script>"_s } },
        { "/destination"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/source"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CanGoBackAfterLoadingAndNavigatingFrame)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='frame' src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { ""_s } },
        { "/destination"_s, { "<script> alert('done'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView canGoBack]);

    __block RetainPtr<WKFrameInfo> childFrameInfo;
    __block bool done = false;
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        childFrameInfo = mainFrame.childFrames.firstObject.info;
        done = true;
    }];
    Util::run(&done);

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/destination'" inFrame:childFrameInfo.get() inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");
    EXPECT_TRUE([webView canGoBack]);
}

}
