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

#pragma once

// Shared by the SiteIsolation*.mm API tests.

#ifdef __cplusplus

#import "Helpers/cocoa/HTTPServer.h"
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <utility>
#import <wtf/RetainPtr.h>
#import <wtf/text/ASCIILiteral.h>

@class TestMessageHandler;
@class TestNavigationDelegate;
@class TestUIDelegate;
@class TestWKWebView;
@class WKFrameInfo;
@class WKWebViewConfiguration;
@class _WKFrameTreeNode;

@interface NavigationDelegateWithUnresponsiveCallback : NSObject<WKNavigationDelegate>
@property (nonatomic, readonly) BOOL didBecomeUnresponsive;
@property (nonatomic, readonly) BOOL didBecomeResponsive;
@property (nonatomic, copy) void (^decidePolicyForNavigationActionWithPreferences)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *));
- (void)waitForDidFinishNavigation;
@end

#if PLATFORM(MAC)
// AppKit responder and Services methods that WKWebView implements but doesn't declare in its headers.
@interface WKWebView (SiteIsolationTestUtilities) <NSServicesMenuRequestor>
- (void)changeAttributes:(id)sender;
- (void)changeSpelling:(id)sender;
- (void)checkSpelling:(id)sender;
@end
#endif

namespace TestWebKitAPI {

void setFeatureEnabled(WKWebViewConfiguration *, NSString *featureName, bool enabled);
void enableSiteIsolation(WKWebViewConfiguration *);
void disableSharedProcess(WKWebViewConfiguration *);

std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration>, CGRect, bool enable);
std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration>, CGRect = CGRectZero);
std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer&, CGRect = CGRectZero);
std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegateWithoutSharedProcess(const HTTPServer&, CGRect = CGRectZero);

enum class EnableProcessCache : bool { No, Yes };
enum class EnableBackForwardCache : bool { No, Yes };
std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewWithSharedProcess(const HTTPServer&,
    EnableProcessCache = EnableProcessCache::No, NSURL *dataStoreDirectory = nil, NSURL *itpRoot = nil, NSString *domainsWithUserInteraction = nil,
    EnableBackForwardCache = EnableBackForwardCache::No);

// Uses a site-isolated web view whose navigation delegate applies the given policy changes to main frame navigations only.
std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> mainFrameOnlyPolicyViewAndDelegate(const HTTPServer&, void (^applyToMainFramePolicy)(WKWebpagePreferences *), WKWebViewConfiguration * = nil);
RetainPtr<WKFrameInfo> loadAndWaitForCrossSiteChildFrame(TestWKWebView *, TestNavigationDelegate *, NSString *mainFrameURL, NSString *childFrameHost);
HTTPServer::ResponseMap mainAndSubframeResponses();

struct WebViewAndDelegates {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestMessageHandler> messageHandler;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<TestUIDelegate> uiDelegate;
};

std::pair<WebViewAndDelegates, WebViewAndDelegates> openerAndOpenedViews(const HTTPServer&, NSString *url = @"https://example.com/example", bool waitForOpenedNavigation = true);

bool processStillRunning(pid_t);
RetainPtr<NSSet> frameTrees(WKWebView *);

enum class FrameType : bool { Local, Remote };
pid_t findFramePID(NSSet<_WKFrameTreeNode *> *, FrameType);

void scrollFrameAndWait(TestWKWebView *, WKFrameInfo *, int scrollX, int scrollY);
void scrollFrameAndWait(TestWKWebView *, WKFrameInfo *, int scrollY);

void insertTextInFrame(TestWKWebView *, WKFrameInfo *, NSString *editableElement, NSString *text);
bool waitForTextContentInFrame(TestWKWebView *, WKFrameInfo *, NSString *editableElement, NSString *text);

RetainPtr<WKWebViewConfiguration> configurationWithInternals(const HTTPServer&);

// initial-scale=1 keeps CSS pixels equal to web-view coordinates, which the helper below relies on.
static constexpr auto pointSelectionMainFrame = "<meta name='viewport' content='initial-scale=1'><body style='margin: 0'>main frame text<iframe id='iframe' style='position: absolute; left: 100px; top: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;
static constexpr auto pointSelectionIframe = "<body contenteditable style='margin: 0; font: 20px monospace'>hello world</body>"_s;

// A point in web-view coordinates just inside the left edge of the character at `offset` in the iframe's
// first text node, so that the nearest character boundary is unambiguously `offset` itself. The iframe is
// positioned at (100, 100) and nothing is scrolled, so its client coordinates are offset by exactly that.
CGPoint pointAtCharacterInIframe(TestWKWebView *, WKFrameInfo *childFrame, unsigned offset);

// Some main frame text and a 400x300 cross-origin iframe with id 'iframe', loaded from https://webkit.org/iframe.
static constexpr auto mainFrameTextWithCrossOriginIframe = "<body style='margin: 0'>main frame text<iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;

struct WebViewWithFocusedCrossOriginIframe {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<WKFrameInfo> childFrame;
};

// Loads https://example.com/mainframe in a site-isolated web view, then focuses its first child frame,
// which must be an iframe with id 'iframe' (see mainFrameTextWithCrossOriginIframe). A configuration, if
// given, must come from the server's httpsProxyConfiguration().
WebViewWithFocusedCrossOriginIframe webViewWithFocusedCrossOriginIframe(const HTTPServer&, WKWebViewConfiguration * = nil);

// Runs a selection script in the frame, then waits for the UI process's editor state to reflect the new
// selection, since some commands check it before sending anything to a web process.
void setSelectionInFrame(TestWKWebView *, WKFrameInfo *, NSString *script, _WKSelectionAttributes expectedSelection);

} // namespace TestWebKitAPI

#endif // __cplusplus
