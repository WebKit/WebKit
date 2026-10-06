/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <signal.h>

@implementation NavigationDelegateWithUnresponsiveCallback {
    bool _finishedNavigation;
    bool _didBecomeUnresponsive;
    bool _didBecomeResponsive;
}
- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}
- (void)waitForDidFinishNavigation
{
    _finishedNavigation = false;
    TestWebKitAPI::Util::run(&_finishedNavigation);
}
- (BOOL)didBecomeUnresponsive
{
    return _didBecomeUnresponsive;
}
- (BOOL)didBecomeResponsive
{
    return _didBecomeResponsive;
}
- (void)_webViewWebProcessDidBecomeUnresponsive:(WKWebView *)webView
{
    _didBecomeUnresponsive = true;
}
- (void)_webViewWebProcessDidBecomeResponsive:(WKWebView *)webView
{
    _didBecomeResponsive = true;
}
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    if (_decidePolicyForNavigationActionWithPreferences)
        _decidePolicyForNavigationActionWithPreferences(navigationAction, preferences, decisionHandler);
    else
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
}
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _finishedNavigation = true;
}
@end

namespace TestWebKitAPI {

void setFeatureEnabled(WKWebViewConfiguration *configuration, NSString *featureName, bool enabled)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:featureName]) {
            [preferences _setEnabled:enabled forFeature:feature];
            break;
        }
    }
}

void enableSiteIsolation(WKWebViewConfiguration *configuration)
{
    setFeatureEnabled(configuration, @"SiteIsolationEnabled", true);
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect, bool enable)
{
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    if (enable)
        enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:rect configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect)
{
    return siteIsolatedViewAndDelegate(configuration, rect, true);
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer& server, CGRect rect)
{
    return siteIsolatedViewAndDelegate(server.httpsProxyConfiguration(), rect, true);
}

WebViewWithFocusedCrossOriginIframe webViewWithFocusedCrossOriginIframe(const HTTPServer& server, WKWebViewConfiguration *configuration)
{
    auto [webView, navigationDelegate] = configuration ? siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600)) : siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
#if PLATFORM(IOS_FAMILY)
    [webView focusInWindow];
#endif

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }

    return { WTF::move(webView), WTF::move(navigationDelegate), WTF::move(childFrame) };
}

void setSelectionInFrame(TestWKWebView *webView, WKFrameInfo *frame, NSString *script, _WKSelectionAttributes expectedSelection)
{
    [webView objectByEvaluatingJavaScript:script inFrame:frame];
    while (!([webView _selectionAttributes] & expectedSelection))
        Util::spinRunLoop();
}

void disableSharedProcess(WKWebViewConfiguration *configuration)
{
    setFeatureEnabled(configuration, @"SiteIsolationSharedProcessEnabled", false);
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewWithSharedProcess(const HTTPServer& server,
    EnableProcessCache enableProcessCache, NSURL *dataStoreDirectory, NSURL *itpRoot, NSString *domainsWithUserInteraction,
    EnableBackForwardCache enableBackForwardCache)
{
    RetainPtr<_WKWebsiteDataStoreConfiguration> dataStoreConfiguration;
    if (!dataStoreDirectory || !itpRoot)
        dataStoreConfiguration = [[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration];
    else {
        dataStoreConfiguration = [[_WKWebsiteDataStoreConfiguration alloc] initWithDirectory:dataStoreDirectory];
        dataStoreConfiguration.get()._resourceLoadStatisticsDirectory = itpRoot;
    }

    [dataStoreConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [dataStoreConfiguration setAdditionalDomainsWithUserInteractionForTesting:domainsWithUserInteraction];

    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    if (enableProcessCache == EnableProcessCache::Yes) {
        RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        processPoolConfiguration.get().usesWebProcessCache = YES;
        processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
        // These tests assert WebProcessCache process-reuse semantics; disable BFCache so it
        // does not compete with WebProcessCache for the cached processes' lifetime, unless a
        // test explicitly needs both caches enabled together (as Safari has them).
        if (enableBackForwardCache == EnableBackForwardCache::No)
            processPoolConfiguration.get().pageCacheEnabled = NO;
        RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
        [configuration setProcessPool:processPool.get()];
    }

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    enableSiteIsolation(configuration.get());
    setFeatureEnabled(configuration.get(), @"SiteIsolationSharedProcessEnabled", true);
    if (enableBackForwardCache == EnableBackForwardCache::Yes)
        setFeatureEnabled(configuration.get(), @"MultiProcessBackForwardCacheEnabled", true);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegateWithoutSharedProcess(const HTTPServer& server, CGRect rect)
{
    RetainPtr configuration = server.httpsProxyConfiguration();
    disableSharedProcess(configuration.get());
    return siteIsolatedViewAndDelegate(configuration, rect, true);
}

bool processStillRunning(pid_t pid)
{
    return !kill(pid, 0);
}

RetainPtr<NSSet> frameTrees(WKWebView *webView)
{
    __block RetainPtr<NSSet> result;
    [webView _frameTrees:^(NSSet<_WKFrameTreeNode *> *frameTrees) {
        result = frameTrees;
    }];
    while (!result)
        Util::spinRunLoop();
    return result;
}

pid_t findFramePID(NSSet<_WKFrameTreeNode *> *set, FrameType local)
{
    for (_WKFrameTreeNode *node in set) {
        if (node.info._isLocalFrame == (local == FrameType::Local))
            return node.info._processIdentifier;
    }
    EXPECT_FALSE(true);
    return 0;
}

std::pair<WebViewAndDelegates, WebViewAndDelegates> openerAndOpenedViews(const HTTPServer& server, NSString *url, bool waitForOpenedNavigation)
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
    return { WTF::move(opener), WTF::move(opened) };
}

void scrollFrameAndWait(TestWKWebView *webView, WKFrameInfo *frame, int scrollX, int scrollY)
{
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.scrollTo(%d, %d)", scrollX, scrollY] inFrame:frame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.scrollX" inFrame:frame] intValue] == scrollX
            && [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:frame] intValue] == scrollY;
    }));
    [webView waitForNextPresentationUpdate];
}

void scrollFrameAndWait(TestWKWebView *webView, WKFrameInfo *frame, int scrollY)
{
    scrollFrameAndWait(webView, frame, 0, scrollY);
}

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> mainFrameOnlyPolicyViewAndDelegate(const HTTPServer& server, void (^applyToMainFramePolicy)(WKWebpagePreferences *), WKWebViewConfiguration *configuration)
{
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration ?: server.httpsProxyConfiguration(), CGRectMake(0, 0, 800, 600), true);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (action.targetFrame.mainFrame)
            applyToMainFramePolicy(preferences);
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

RetainPtr<WKFrameInfo> loadAndWaitForCrossSiteChildFrame(TestWKWebView *webView, TestNavigationDelegate *navigationDelegate, NSString *mainFrameURL, NSString *childFrameHost)
{
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:mainFrameURL]]];
    [navigationDelegate waitForDidFinishNavigation];
    while (![[webView firstChildFrame].securityOrigin.host isEqualToString:childFrameHost])
        Util::spinRunLoop();
    return [webView firstChildFrame];
}

HTTPServer::ResponseMap mainAndSubframeResponses()
{
    HTTPServer::ResponseMap responses;
    responses.add("/mainframe"_s, HTTPResponse("<!DOCTYPE html><iframe src='https://b.com/subframe'></iframe>"_s));
    responses.add("/subframe"_s, HTTPResponse("<!DOCTYPE html>subframe"_s));
    return responses;
}

void insertTextInFrame(TestWKWebView *webView, WKFrameInfo *frame, NSString *editableElement, NSString *text)
{
    [webView objectByEvaluatingJavaScriptWithUserGesture:[NSString stringWithFormat:@"%@.focus(); document.execCommand('insertText', false, '%@')", editableElement, text] inFrame:frame];

    // Give the platform undo manager a chance to close the group it opened for this edit, so that consecutive edits are undone one at a time.
    [webView waitForNextPresentationUpdate];
}

bool waitForTextContentInFrame(TestWKWebView *webView, WKFrameInfo *frame, NSString *editableElement, NSString *text)
{
    return Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"%@.textContent", editableElement] inFrame:frame] isEqualToString:text];
    });
}

RetainPtr<WKWebViewConfiguration> configurationWithInternals(const HTTPServer& server)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration setWebsiteDataStore:[server.httpsProxyConfiguration() websiteDataStore]];
    return configuration;
}

CGPoint pointAtCharacterInIframe(TestWKWebView *webView, WKFrameInfo *childFrame, unsigned offset)
{
    RetainPtr script = [NSString stringWithFormat:@"(() => {"
        "let range = document.createRange();"
        "range.setStart(document.body.firstChild, %u);"
        "range.setEnd(document.body.firstChild, %u);"
        "let rect = range.getBoundingClientRect();"
        "return [rect.left + 2, rect.top + rect.height / 2];"
        "})()", offset, offset + 1];
    RetainPtr result = [webView objectByEvaluatingJavaScript:script.get() inFrame:childFrame];
    return CGPointMake(100 + [[result objectAtIndex:0] doubleValue], 100 + [[result objectAtIndex:1] doubleValue]);
}

} // namespace TestWebKitAPI
