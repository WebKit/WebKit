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
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import "WKWebViewFindStringFindDelegate.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/MakeString.h>

@interface TestObserver : NSObject

@property (nonatomic, copy) void (^observeValueForKeyPath)(NSString *, id);

@end

@implementation TestObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    _observeValueForKeyPath(keyPath, object);
}

@end

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

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect = CGRectZero, bool enable = true)
{
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    if (enable)
        enableSiteIsolation(configuration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:rect configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTFMove(webView), WTFMove(navigationDelegate) };
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer& server, CGRect rect = CGRectZero)
{
    return siteIsolatedViewAndDelegate(server.httpsProxyConfiguration(), rect);
}

static bool processStillRunning(pid_t pid)
{
    return !kill(pid, 0);
}

static bool frameTreesMatch(_WKFrameTreeNode *actualRoot, ExpectedFrameTree&& expectedRoot)
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
    for (_WKFrameTreeNode *actualChild in actualRoot.childFrames) {
        auto index = expectedRoot.children.findIf([&] (auto& expectedFrameTree) {
            return frameTreesMatch(actualChild, ExpectedFrameTree { expectedFrameTree });
        });
        if (index == WTF::notFound)
            return false;
        expectedRoot.children.remove(index);
    }
    return expectedRoot.children.isEmpty();
}

static bool frameTreesMatch(NSSet<_WKFrameTreeNode *> *actualFrameTrees, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    if (actualFrameTrees.count != expectedFrameTrees.size())
        return false;

    for (_WKFrameTreeNode *root in actualFrameTrees) {
        auto index = expectedFrameTrees.findIf([&] (auto& expectedFrameTree) {
            return frameTreesMatch(root, ExpectedFrameTree { expectedFrameTree });
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

static void checkFrameTreesInProcesses(NSSet<_WKFrameTreeNode *> *actualTrees, const Vector<ExpectedFrameTree>& expectedFrameTrees)
{
    bool result = frameTreesMatch(actualTrees, Vector<ExpectedFrameTree> { expectedFrameTrees });
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
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestMessageHandler> messageHandler;
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
        opened.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
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

TEST(SiteIsolation, OpenWithNoopener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit', '_blank', 'noopener')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    __block RetainPtr openerView = opener.webView;
    __block RetainPtr openedView = opened.webView;
    opened.navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^completionHandler)(WKNavigationActionPolicy)) {
        checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
        checkFrameTreesInProcesses(openedView.get(), { { "://"_s } }); // FIXME: This should be https://webkit.org
        EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
        completionHandler(WKNavigationActionPolicyAllow);
    };
    opened.navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
        checkFrameTreesInProcesses(openedView.get(), { { "://"_s } }); // FIXME: This should be https://webkit.org
        EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
        completionHandler(WKNavigationResponsePolicyAllow);
    };
    [opened.navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(openedView.get(), { { "https://webkit.org"_s } });
    EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
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

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    __block bool opened { false };
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures)
    {
        opened = true;
        return nil;
    };
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"window.open('https://example.com/opened')" inFrame:[webView firstChildFrame] completionHandler:nil];
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

    [opened.webView evaluateJavaScript:@"try { opener.postMessage('test1', '*'); alert('posted message 1') } catch(e) { alert(e) }" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "posted message 1");

    [opened.webView evaluateJavaScript:@"try { top.opener.postMessage('test2', '*'); alert('posted message 2') } catch(e) { alert(e) }" inFrame:[opened.webView firstChildFrame] completionHandler:nil];
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

    EXPECT_FALSE([[opener.webView objectByEvaluatingJavaScript:@"w.closed"] boolValue]);
    [opener.webView evaluateJavaScript:@"w.close()" completionHandler:nil];
    [opened.uiDelegate waitForDidClose];
    [opened.webView _close];
    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    EXPECT_TRUE([[opener.webView objectByEvaluatingJavaScript:@"w.closed"] boolValue]);
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

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
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

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
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

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
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
    EXPECT_EQ(3u, [webView mainFrame].childFrames.count);

    [webView synchronouslyLoadRequest:server.request("/page2.html"_s)];
    EXPECT_EQ(1u, [webView mainFrame].childFrames.count);
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");

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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "foo.com");
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
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
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:firstWebView.get().configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto firstWebViewMainFrame = [firstWebView mainFrame];
    EXPECT_NE(firstWebViewMainFrame.info._processIdentifier, 0);
    pid_t firstFramePID = firstWebViewMainFrame.info._processIdentifier;
    EXPECT_WK_STREQ(firstWebViewMainFrame.info.securityOrigin.host, "webkit.org");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_EQ(firstFramePID, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
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

    auto mainFrame = [webView mainFrame];
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = [webView firstChildFrame];
    EXPECT_NE(mainFrame.info._processIdentifier, childFrame._processIdentifier);

    [webView evaluateJavaScript:@"document.getElementById('webkit_frame').src = 'https://example.com/example_subframe'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });

    while (processStillRunning(childFrame._processIdentifier))
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

    auto mainFrame = [webView mainFrame];
    auto childFrame = [webView firstChildFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.securityOrigin.host, "webkit.org");

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
    [webView waitForNextPresentationUpdate];
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
    "    alert('iframe loaded');"
    "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    EXPECT_WK_STREQ("iframe loaded", [webView _test_waitForAlert]);

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(50, 50) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];

    NSArray<NSString *> *eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    while (eventTypes.count != 3u)
        eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    EXPECT_WK_STREQ("mousemove", eventTypes[0]);
    EXPECT_WK_STREQ("mousedown,40,40", eventTypes[1]);
    EXPECT_WK_STREQ("mouseup,40,40", eventTypes[2]);
}

TEST(SiteIsolation, RunOpenPanel)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html><input style='width: 100vw; height: 100vh;' type='file'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    __block bool fileSelected = false;
    [uiDelegate setRunOpenPanelWithParameters:^(WKWebView *, WKOpenPanelParameters *, WKFrameInfo *, void (^completionHandler)(NSArray<NSURL *> *)) {
        fileSelected = true;
        completionHandler(@[ [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"test"]] ]);
    }];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(100, 100) toView:nil];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];
    Util::run(&fileSelected);

    NSString *js = @"function f() { try { return document.getElementsByTagName('input')[0].files[0].name } catch (e) { return 'exception: ' + e; } }; f()";
    while (![[webView objectByEvaluatingJavaScript:js inFrame:[webView firstChildFrame]] isEqualToString:@"test"])
        Util::spinRunLoop();
}

TEST(SiteIsolation, CancelOpenPanel)
{
    auto subframeHTML = "<!DOCTYPE html><input style='width: 100vw; height: 100vh;' id='file' type='file'>"
        "<script>"
        "document.getElementById('file').addEventListener('cancel', () => { alert('cancel'); });"
        "</script>"_s;
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    [uiDelegate setRunOpenPanelWithParameters:^(WKWebView *, WKOpenPanelParameters *, WKFrameInfo *, void (^completionHandler)(NSArray<NSURL *> *)) {
        completionHandler(nil);
    }];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(100, 100) toView:nil];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "cancel");
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

TEST(SiteIsolation, FrameMetrics)
{
    auto mainframeHTML = "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div>"
    "Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc."
    "</div>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr frame = [webView mainFrame];
    WKFrameInfo *info = frame.get().info;

    EXPECT_EQ(info._isScrollable, NO);
    EXPECT_EQ(info._contentSize.width, 800);
    EXPECT_EQ(info._contentSize.height, 600);
    EXPECT_TRUE(CGSizeEqualToSize(info._contentSize, info._visibleContentSize));
    EXPECT_TRUE(CGSizeEqualToSize(info._contentSize, info._visibleContentSizeExcludingScrollbars));

    frame = frame.get().childFrames.firstObject;
    info = frame.get().info;

    EXPECT_EQ(info._isScrollable, YES);
    EXPECT_EQ(info._visibleContentSize.width, 300);
    EXPECT_EQ(info._visibleContentSize.height, 300);
    EXPECT_EQ(info._visibleContentSize.height, info._visibleContentSizeExcludingScrollbars.height);
    EXPECT_EQ(info._visibleContentSizeExcludingScrollbars.width, info._contentSize.width);
    EXPECT_TRUE(info._visibleContentSizeExcludingScrollbars.width < info._visibleContentSize.width);
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
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(20, 20) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    auto *data = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"sunset-in-cupertino-400px" ofType:@"gif"]];
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

    __block RetainPtr<TestWKWebView> openedWebView;
    __block auto uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
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

    auto openerMainFrame = [webView mainFrame];
    auto openedMainFrame = [openedWebView mainFrame];
    pid_t openerMainFramePid = openerMainFrame.info._processIdentifier;
    pid_t openedMainFramePid = openedMainFrame.info._processIdentifier;
    pid_t openerIframePid = openerMainFrame.childFrames.firstObject.info._processIdentifier;
    pid_t openedIframePid = openedMainFrame.childFrames.firstObject.info._processIdentifier;

    EXPECT_EQ(openerMainFramePid, openedMainFramePid);
    EXPECT_NE(openerMainFramePid, 0);
    EXPECT_EQ(openerIframePid, openedIframePid);
    EXPECT_NE(openerIframePid, 0);
}

#if PLATFORM(MAC)
TEST(SiteIsolation, AppKitText)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<html><body><input id='input' value='test'></input></body></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrameInfo = [webView firstChildFrame];
    auto textLocation = NSMakePoint(23, 564);
    while ("TEST"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView uppercaseWord:nil];
        Util::runFor(10_ms);
    }
    while ("test"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView lowercaseWord:nil];
        Util::runFor(10_ms);
    }
    while ("Test"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView capitalizeWord:nil];
        Util::runFor(10_ms);
    }
}
#endif

TEST(SiteIsolation, SetFocusedFrame)
{
    auto mainframeHTML = "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView mainFrame].info._isFocused);
    EXPECT_FALSE([webView firstChildFrame]._isFocused);

    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while ([webView mainFrame].info._isFocused || ![webView firstChildFrame]._isFocused)
        Util::spinRunLoop();

    [webView evaluateJavaScript:@"window.focus()" completionHandler:nil];
    while (![webView mainFrame].info._isFocused || [webView firstChildFrame]._isFocused)
        Util::spinRunLoop();
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
    EXPECT_WK_STREQ("abc", [webView objectByEvaluatingJavaScript:@"window.test" inFrame:[webView firstChildFrame]]);
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

    auto canLoadURLInIFrame = [childFrame = RetainPtr { [webView firstChildFrame] }, webView = RetainPtr { webView }] (NSString *path) -> bool {
        __block std::optional<bool> loadedSuccessfully;
        [webView callAsyncJavaScript:[NSString stringWithFormat:@"try { let response = await fetch('%@'); return await response.text() } catch (e) { return 'load failed' }", path] arguments:nil inFrame:childFrame.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
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
    EXPECT_FALSE([[[opener.webView mainFrame] info] _isFocused]);
    EXPECT_FALSE([[[opened.webView mainFrame] info] _isFocused]);

    [opener.webView.get() evaluateJavaScript:@"w.focus()" completionHandler:nil];
    while (![[[opened.webView mainFrame] info] _isFocused])
        Util::spinRunLoop();
    EXPECT_FALSE([[[opener.webView mainFrame] info] _isFocused]);
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
    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    EXPECT_FALSE([[webView findStringAndWait:@"Missing string" withConfiguration:findConfiguration.get()] matchFound]);
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
    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    EXPECT_FALSE([[webView findStringAndWait:@"Missing string" withConfiguration:findConfiguration.get()] matchFound]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[2].info]);
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
        [[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound];
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[3].first endOffset:offsets[3].second inFrame:mainFrame.childFrames[0].childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[4].first endOffset:offsets[4].second inFrame:mainFrame.childFrames[1].childFrames[0].info]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
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
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
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

    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
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

TEST(SiteIsolation, OpenProvisionalFailure)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    [opened.navigationDelegate waitForDidFailProvisionalNavigation];
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://example.com"_s } });
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

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_example_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_example_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_example_terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_webkit_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_webkit_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_webkit_terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_apple_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_apple_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_apple_terminate");
}

TEST(SiteIsolation, DrawAfterNavigateToDomainAgain)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/b'></iframe>"_s } },
        { "/b"_s, { "hi"_s } },
        { "/c"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://b.com"_s } }
        }
    });

    [webView evaluateJavaScript:@"window.location = 'https://c.com/c'" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://c.com"_s }
    });

    [webView evaluateJavaScript:@"window.location = 'https://a.com/a'" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://b.com"_s } }
        }
    });

    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CancelProvisionalLoad)
{
    HTTPServer server({
        { "/main"_s, {
            "<iframe id='testiframe' src='https://example.com/respond_quickly'></iframe>"
            "<iframe src='https://example.com/respond_quickly'></iframe>"_s
        } },
        { "/respond_quickly"_s, { "hi"_s } },
        { "/never_respond"_s, { HTTPResponse::Behavior::NeverSendResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    auto checkStateAfterSequentialFrameLoads = [webView = RetainPtr { webView }, navigationDelegate = RetainPtr { navigationDelegate }] (NSString *first, NSString *second, Vector<ExpectedFrameTree>&& expectedTrees) {
        [webView evaluateJavaScript:[NSString stringWithFormat:@"i = document.getElementById('testiframe'); i.addEventListener('load', () => { alert('iframe loaded') }); i.src = '%@'; setTimeout(()=>{ i.src = '%@' }, Math.random() * 100)", first, second] completionHandler:nil];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "iframe loaded");
        checkFrameTreesInProcesses(webView.get(), WTFMove(expectedTrees));
    };

    checkStateAfterSequentialFrameLoads(@"https://webkit.org/never_respond", @"https://example.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://example.com/never_respond", @"https://webkit.org/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { "https://webkit.org"_s } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://webkit.org/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { "https://webkit.org"_s } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://example.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://apple.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        }, { RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s } }
        }
    });
}

// FIXME: If a provisional load happens in a RemoteFrame with frame children, does anything clear out those
// child frames when the load commits? Probably not. Needs a test.

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

    [webView setCustomUserAgent:@"Custom UserAgent"];
    EXPECT_WK_STREQ(@"Custom UserAgent", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);
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

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]] hasSuffix:@" Custom UserAgent"]);
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

    EXPECT_WK_STREQ("Custom UserAgent", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    Util::run(&receivedRequestFromSubframe);
    EXPECT_WK_STREQ("Custom UserAgent2", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);
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

TEST(SiteIsolation, WebsitePoliciesCustomNavigatorPlatform)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomNavigatorPlatform:@"Custom Navigator Platform"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ("Custom Navigator Platform", [webView objectByEvaluatingJavaScript:@"navigator.platform" inFrame:[webView firstChildFrame]]);
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

    __block bool done = false;
    navigationDelegate.get().didFailProvisionalLoadWithRequestInFrameWithError = ^(WKWebView *, NSURLRequest *request, WKFrameInfo *, NSError *) {
        EXPECT_WK_STREQ(request.URL.absoluteString, "https://example.com/terminate");
        done = true;
    };
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/redirect'" inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    Util::run(&done);
}

TEST(SiteIsolation, SynchronouslyExecuteEditCommandSelectAll)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe' src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "test"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
        Util::spinRunLoop();
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

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView selectAll:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
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
    EXPECT_EQ(-10, [[webView objectByEvaluatingJavaScript:@"window.innerHeight"] intValue]);
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

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/destination'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");
    EXPECT_TRUE([webView canGoBack]);
}

TEST(SiteIsolation, CanGoBackAfterNavigatingFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='frame' src='https://domain1.com/source'></iframe>"_s } },
        { "/source"_s, { ""_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"location.href = 'https://domain2.com/destination'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");
    EXPECT_TRUE([webView canGoBack]);
}

TEST(SiteIsolation, NavigateIframeSameOriginBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "source");

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/destination'" inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");

    [webView goBack];
    EXPECT_WK_STREQ("source", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/source", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);

    [webView goForward];
    EXPECT_WK_STREQ("destination", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/destination", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);
}

TEST(SiteIsolation, NavigateIframeCrossOriginBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://a.com/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    [webView evaluateJavaScript:@"location.href = 'https://b.com/b'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
}

TEST(SiteIsolation, ProtocolProcessSeparation)
{
    HTTPServer secureServer({
        { "/subdomain"_s, { "hi"_s } },
        { "/no_subdomain"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    HTTPServer plaintextServer({
        { "http://a.com/"_s, {
            "<iframe src='https://a.com/no_subdomain'></iframe>"
            "<iframe src='https://subdomain.a.com/subdomain'></iframe>"_s
        } }
    });

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", secureServer.port()]]];
    [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", plaintextServer.port()]]];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://a.com/"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "http://a.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://subdomain.a.com"_s }, { "https://a.com"_s } }
        },
    });
}

TEST(SiteIsolation, GoBackToPageWithIframe)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/b"_s, { ""_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://frame.com"_s } }
        },
    });
}

TEST(SiteIsolation, NavigateNestedIframeSameOriginBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://a.com/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    RetainPtr<WKFrameInfo> childFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info;
    [webView evaluateJavaScript:@"location.href = 'https://a.com/b'" inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
}

TEST(SiteIsolation, AdvancedPrivacyProtectionsHideScreenMetricsFromBindings)
{
    auto frameHTML = [NSString stringWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"simple" ofType:@"html"] encoding:NSUTF8StringEncoding error:NULL];
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/frame"_s, { frameHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry | _WKWebsiteNetworkConnectionIntegrityPolicyEnabled];
    [configuration setDefaultWebpagePreferences:preferences.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenX" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenY" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availLeft" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availTop" inFrame:childFrame.get()] intValue]);
}

TEST(SiteIsolation, UpdateWebpagePreferences)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setCustomUserAgent:@"Custom UserAgent"];
    [webView _updateWebpagePreferences:preferences.get()];
    while (![[webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]] isEqualToString:@"Custom UserAgent"])
        Util::spinRunLoop();
}

TEST(SiteIsolation, MainFrameRedirectBetweenExistingProcesses)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/webkit_redirect"_s, { 302, { { "Location"_s, "https://example.com/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.length"] intValue], 1);
    auto pidBefore = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org/webkit_redirect"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.length"] intValue], 0);
    EXPECT_EQ([webView _webProcessIdentifier], pidBefore);
}

TEST(SiteIsolation, URLSchemeTask)
{
    HTTPServer server({
        { "/example"_s, { ""_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/example"])
            respond(task, "<iframe src='customscheme://webkit.org/webkit'></iframe>");
        else if ([task.request.URL.path isEqualToString:@"/webkit"]) {
            respond(task, "<script>"
                "var xhr = new XMLHttpRequest();"
                "xhr.open('GET', '/fetched');"
                "xhr.onreadystatechange = function () {"
                    "if (xhr.readyState == xhr.DONE) { alert(xhr.responseURL + ' ' + xhr.responseText) }"
                "};"
                "xhr.send();"
            "</script>");
        } else if ([task.request.URL.path isEqualToString:@"/fetched"]) {
            auto newRequest = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"customscheme://webkit.org/redirected"]]);
            [(id<WKURLSchemeTaskPrivate>)task _willPerformRedirection:adoptNS([NSURLResponse new]).get() newRequest:newRequest.get() completionHandler:^(NSURLRequest *request) {
                respond(task, "hi");
            }];
        } else
            EXPECT_TRUE(false);
    };
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"customscheme"];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "customscheme://webkit.org/redirected hi");
    checkFrameTreesInProcesses(webView.get(), {
        { "customscheme://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "customscheme://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, ThemeColor)
{
    HTTPServer server({
        { "/example"_s, { "<meta name='theme-color' content='red'><iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<meta name='theme-color' content='blue'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server);
    EXPECT_FALSE([webView themeColor]);

    __block bool observed { false };
    auto observer = adoptNS([TestObserver new]);
    observer.get().observeValueForKeyPath = ^(NSString *path, id view) {
        EXPECT_WK_STREQ(path, "themeColor");

        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        EXPECT_TRUE(CGColorEqualToColor([[view themeColor] CGColor], redColor.get()));
        observed = true;
    };
    [webView.get() addObserver:observer.get() forKeyPath:@"themeColor" options:NSKeyValueObservingOptionNew context:nil];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    Util::run(&observed);
    Util::runFor(0.1_s);
}

static WebViewAndDelegates makeWebViewAndDelegates(HTTPServer& server)
{
    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    RetainPtr configuration = server.httpsProxyConfiguration();
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get());
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    return {
        WTFMove(webView),
        WTFMove(messageHandler),
        WTFMove(navigationDelegate),
        WTFMove(uiDelegate)
    };
};

TEST(SiteIsolation, SandboxFlags)
{
    NSString *checkAlertJS = @"alert('alerted');window.open('https://example.com/opened');window.webkit.messageHandlers.testHandler.postMessage('testHandler')";

    HTTPServer server({
        { "/example"_s, { "<iframe sandbox='allow-scripts' id='testiframe' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/check-when-loaded"_s, { [NSString stringWithFormat:@"<script>onload = ()=>{ %@ }</script>", checkAlertJS] } },
        { "/csp-forbids-alert"_s, { { { "Content-Security-Policy"_s, "sandbox allow-scripts"_s } }, "<script>alert('alerted');window.webkit.messageHandlers.testHandler.postMessage('testHandler')</script>"_s } },
        { "/opened"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    bool receivedMessage { false };
    bool receivedAlert { false };
    bool receivedOpen { false };
    auto reset = [&] {
        receivedMessage = false;
        receivedAlert = false;
        receivedOpen = false;
    };

    WebViewAndDelegates openedWebViewAndDelegates;
    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [webViewAndDelegates.messageHandler addMessage:@"testHandler" withHandler:[&] {
        receivedMessage = true;
    }];
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = [&](WKWebView *, NSString *alert, WKFrameInfo *, void (^completionHandler)()) {
        receivedAlert = true;
        completionHandler();
    };
    auto returnNilOpenedView = [&] (WKWebViewConfiguration *, WKNavigationAction *, WKWindowFeatures *) -> WKWebView * {
        receivedOpen = true;
        return nil;
    };
    auto returnNonNilOpenedView = [&] (WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) -> WKWebView * {
        EXPECT_FALSE(openedWebViewAndDelegates.webView);
        openedWebViewAndDelegates = WebViewAndDelegates {
            adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]),
            nil,
            adoptNS([TestNavigationDelegate new]),
            uiDelegate
        };
        openedWebViewAndDelegates.webView.get().UIDelegate = uiDelegate.get();
        openedWebViewAndDelegates.webView.get().navigationDelegate = openedWebViewAndDelegates.navigationDelegate.get();
        [openedWebViewAndDelegates.navigationDelegate allowAnyTLSCertificate];
        receivedOpen = true;
        return openedWebViewAndDelegates.webView.get();
    };
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [webViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"let i = document.getElementById('testiframe'); i.sandbox = 'allow-scripts allow-modals'" completionHandler:^(id, NSError *) {
        [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    }];
    Util::run(&receivedMessage);
    // The second warning of https://html.spec.whatwg.org/multipage/iframe-embed-object.html#attr-iframe-sandbox
    // says we shouldn't change the effective sandbox until an iframe navigates.
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://example.org/csp-forbids-alert'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://example.org/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.removeAttribute('sandbox'); i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNonNilOpenedView;
    [webView evaluateJavaScript:@"i.sandbox = 'allow-scripts allow-popups'; i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    while (!openedWebViewAndDelegates.webView)
        Util::spinRunLoop();
    [openedWebViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;
    [openedWebViewAndDelegates.webView evaluateJavaScript:checkAlertJS completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNonNilOpenedView;
    openedWebViewAndDelegates.webView = nil;
    [webView evaluateJavaScript:@"i.sandbox = 'allow-scripts allow-popups allow-popups-to-escape-sandbox'; i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    while (!openedWebViewAndDelegates.webView)
        Util::spinRunLoop();
    [openedWebViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;
    [openedWebViewAndDelegates.webView evaluateJavaScript:checkAlertJS completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_TRUE(receivedOpen);
}

TEST(SiteIsolation, SandboxFlagsDuringNavigation)
{
    bool receivedIframe2Request { false };
    HTTPServer server { HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (true) {
            auto path = HTTPServer::parsePath(co_await connection.awaitableReceiveHTTPRequest());
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse("<iframe sandbox='allow-scripts' id='testiframe' src='https://webkit.org/iframe1'></iframe>"_s).serialize());
                continue;
            }
            if (path == "/iframe1"_s) {
                co_await connection.awaitableSend(HTTPResponse("hi"_s).serialize());
                continue;
            }
            if (path == "/iframe2"_s) {
                receivedIframe2Request = true;
                // Never respond.
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy };

    NSString *checkAlertJS = @"alert('alerted');window.webkit.messageHandlers.testHandler.postMessage('testHandler')";

    bool receivedMessage { false };
    bool receivedAlert { false };
    auto reset = [&] {
        receivedMessage = false;
        receivedAlert = false;
        receivedIframe2Request = false;
    };

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    webViewAndDelegates.uiDelegate.get().runJavaScriptAlertPanelWithMessage = [&](WKWebView *, NSString *alert, WKFrameInfo *, void (^completionHandler)()) {
        receivedAlert = true;
        completionHandler();
    };
    [webViewAndDelegates.messageHandler addMessage:@"testHandler" withHandler:[&] {
        receivedMessage = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [webViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);

    reset();
    [webView evaluateJavaScript:@"let i = document.getElementById('testiframe'); i.sandbox = 'allow-scripts allow-modals'; i.src='https://webkit.org/iframe2'" completionHandler:nil];
    Util::run(&receivedIframe2Request);
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
}

TEST(SiteIsolation, NavigateNestedRootFramesBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    RetainPtr<_WKFrameTreeNode> nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;
    [webView evaluateJavaScript:@"location.href = 'https://a.com/b'" inFrame:[nestedChildFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
}

TEST(SiteIsolation, NavigateFrameWithSiblingsBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe> <iframe src='https://webkit.org/b'></iframe>"_s } },
        { "/a"_s, { ""_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");

    RetainPtr<_WKFrameTreeNode> secondRootFrame = [webView mainFrame].childFrames[1];
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/c'" inFrame:[secondRootFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
}

TEST(SiteIsolation, RedirectToCSP)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/initial'></iframe>"_s } },
        { "/initial"_s, { 302, { { "Location"_s, "https://example.org/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { { { "Content-Type"_s, "text/html"_s }, { "Content-Security-Policy"_s, "frame-ancestors 'none'"_s } }, "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
}

TEST(SiteIsolation, MultipleWebViewsWithSameOpenedConfiguration)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='/iframe'></iframe>"_s } },
        { "/iframe"_s, {
            "<script>onload = () => { document.getElementById('mylink').click() }</script>"
            "<a href='/popup' target='_blank' id='mylink'>link</a>"_s
        } },
        { "/popup"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:opened.webView.get().configuration]);
    [opened.navigationDelegate waitForDidFinishNavigation];
    // FIXME: load something with webView2 without asserting, like https://example.com/popup
}

TEST(SiteIsolation, RecoverFromCrash)
{
    HTTPServer server({
        { "/crash"_s, { "<script>window.internals.terminateWebContentProcess()</script>"_s } },
        { "/dontcrash"_s, { "hi"_s } },
        { "/iframecrash"_s, { "<iframe src='https://webkit.org/crash'></iframe>"_s } },
        { "/iframedontcrash"_s, { "<iframe src='https://webkit.org/dontcrash'></iframe>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/crash"]]];
    [navigationDelegate waitForWebContentProcessDidTerminate];
    [webView reload];
    [navigationDelegate waitForWebContentProcessDidTerminate];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/iframecrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/iframedontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
}

TEST(SiteIsolation, IframeOpener)
{
    auto mainFrameHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('main frame received ' + event.data)"
    "    }, false);"
    "    onload = () => { window.open('https://example.com/iframe', 'myframename') }"
    "</script>"
    "<iframe name='myframename'></iframe>"_s;

    auto iframeHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "    try { window.opener.postMessage('hello', '*') } catch (e) { alert('error ' + e) }"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { mainFrameHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    auto verifyThatOpenerIsParent = [webView = RetainPtr { webView }] (bool openerShouldBeParent) {
        auto value = openerShouldBeParent ? "1" : "0";
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.frames[0].opener == self"], value);
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.opener == window.parent" inFrame:[webView firstChildFrame]], value);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "child frame received hello");
    verifyThatOpenerIsParent(false);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);
}

}
