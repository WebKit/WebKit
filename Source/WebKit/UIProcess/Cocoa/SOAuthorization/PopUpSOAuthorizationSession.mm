/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#import "PopUpSOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "APINavigationAction.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/ResourceResponse.h>
#import <wtf/BlockPtr.h>

@interface WKSOSecretDelegate : NSObject <WKNavigationDelegate, WKUIDelegate> {
@private
    ThreadSafeWeakPtr<WebKit::PopUpSOAuthorizationSession> _weakSession;
    BOOL _isFirstNavigation;
}

- (instancetype)initWithSession:(WebKit::PopUpSOAuthorizationSession&)session;

@end

@implementation WKSOSecretDelegate

- (instancetype)initWithSession:(WebKit::PopUpSOAuthorizationSession&)session
{
    if ((self = [super init])) {
        _weakSession = session;
        _isFirstNavigation = YES;
    }
    return self;
}

// WKUIDelegate
- (void)webViewDidClose:(WKWebView *)webView
{
    auto strongSession = _weakSession.get();
    if (!strongSession)
        return;
    strongSession->close(webView);
}

// WKNavigationDelegate
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // FIXME<rdar://problem/48787839>: We should restrict the load to only substitute data.
    // Use the following heuristic as a workaround right now.
    // Ignore the first load in the secret window, which navigates to the authentication URL.
    if (_isFirstNavigation) {
        _isFirstNavigation = NO;
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }
    decisionHandler(_WKNavigationActionPolicyAllowWithoutTryingAppLink);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    auto strongSession = _weakSession.get();
    if (!strongSession)
        return;
    strongSession->close(webView);
}

@end

#define AUTHORIZATIONSESSION_RELEASE_LOG(fmt, ...) RELEASE_LOG(AppSSO, "%p - [InitiatingAction=%s][State=%s] PopUpSOAuthorizationSession::" fmt, this, initiatingActionString(), stateString(), ##__VA_ARGS__)

namespace WebKit {

Ref<SOAuthorizationSession> PopUpSOAuthorizationSession::create(RetainPtr<WKSOAuthorizationDelegate> delegate, WebPageProxy& page, Ref<API::NavigationAction>&& navigationAction, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
{
    return adoptRef(*new PopUpSOAuthorizationSession(delegate, page, WTFMove(navigationAction), WTFMove(newPageCallback), WTFMove(uiClientCallback)));
}

PopUpSOAuthorizationSession::PopUpSOAuthorizationSession(RetainPtr<WKSOAuthorizationDelegate> delegate, WebPageProxy& page, Ref<API::NavigationAction>&& navigationAction, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
    : SOAuthorizationSession(delegate, WTFMove(navigationAction), page, InitiatingAction::PopUp)
    , m_newPageCallback(WTFMove(newPageCallback))
    , m_uiClientCallback(WTFMove(uiClientCallback))
{
}

PopUpSOAuthorizationSession::~PopUpSOAuthorizationSession()
{
    ASSERT(state() != State::Waiting);
    if (m_newPageCallback)
        m_newPageCallback(nullptr);
}

void PopUpSOAuthorizationSession::shouldStartInternal()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("shouldStartInternal: m_page=%p", page());
    ASSERT(page() && page()->isInWindow());
    start();
}

void PopUpSOAuthorizationSession::fallBackToWebPathInternal()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("fallBackToWebPathInternal");
    m_uiClientCallback(releaseNavigationAction(), WTFMove(m_newPageCallback));
}

void PopUpSOAuthorizationSession::abortInternal()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("abortInternal: m_page=%p", page());
    if (!page()) {
        m_newPageCallback(nullptr);
        return;
    }

    initSecretWebView();
    if (!m_secretWebView) {
        m_newPageCallback(nullptr);
        return;
    }

    m_newPageCallback(m_secretWebView->_page.get());
    [m_secretWebView evaluateJavaScript: @"window.close()" completionHandler:nil];
}

void PopUpSOAuthorizationSession::completeInternal(const WebCore::ResourceResponse& response, NSData *data)
{
    AUTHORIZATIONSESSION_RELEASE_LOG("completeInternal: httpState=%d", response.httpStatusCode());
    if (response.httpStatusCode() != 200 || !page()) {
        fallBackToWebPathInternal();
        return;
    }

    initSecretWebView();
    if (!m_secretWebView) {
        fallBackToWebPathInternal();
        return;
    }

    m_newPageCallback(m_secretWebView->_page.get());
    [m_secretWebView loadData:data MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:response.url()];
}

void PopUpSOAuthorizationSession::close(WKWebView *webView)
{
    AUTHORIZATIONSESSION_RELEASE_LOG("close");
    if (!m_secretWebView)
        return;
    if (state() != State::Completed || webView != m_secretWebView.get()) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_secretWebView = nullptr;
    WTFLogAlways("SecretWebView is cleaned.");
}

void PopUpSOAuthorizationSession::initSecretWebView()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("initSecretWebView");
    ASSERT(page());
    auto initiatorWebView = page()->cocoaView();
    if (!initiatorWebView)
        return;
    auto configuration = adoptNS([[initiatorWebView configuration] copy]);
    [configuration _setRelatedWebView:initiatorWebView.get()];
    auto secretViewPreferences = adoptNS([[configuration preferences] copy]);
    [secretViewPreferences _setExtensibleSSOEnabled:NO];
    [configuration setPreferences:secretViewPreferences.get()];
    m_secretWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    m_secretDelegate = adoptNS([[WKSOSecretDelegate alloc] initWithSession:*this]);
    [m_secretWebView setUIDelegate:m_secretDelegate.get()];
    [m_secretWebView setNavigationDelegate:m_secretDelegate.get()];

    RELEASE_ASSERT(!m_secretWebView->_page->preferences().isExtensibleSSOEnabled());
    WTFLogAlways("SecretWebView is created.");
}

} // namespace WebKit

#undef AUTHORIZATIONSESSION_RELEASE_LOG

#endif
