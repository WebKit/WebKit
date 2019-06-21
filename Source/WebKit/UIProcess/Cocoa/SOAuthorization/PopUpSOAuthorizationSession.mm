/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "WKNavigationDelegatePrivate.h"
#import "WKUIDelegate.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import <WebCore/ResourceResponse.h>
#import <WebKit/APINavigationAction.h>
#import <wtf/BlockPtr.h>

@interface WKSOSecretDelegate : NSObject <WKNavigationDelegate, WKUIDelegate> {
@private
    WeakPtr<WebKit::PopUpSOAuthorizationSession> _session;
    BOOL _isFirstNavigation;
}

- (instancetype)initWithSession:(WebKit::PopUpSOAuthorizationSession *)session;

@end

@implementation WKSOSecretDelegate

- (instancetype)initWithSession:(WebKit::PopUpSOAuthorizationSession *)session
{
    if ((self = [super init])) {
        _session = makeWeakPtr(session);
        _isFirstNavigation = YES;
    }
    return self;
}

// WKUIDelegate
- (void)webViewDidClose:(WKWebView *)webView
{
    if (!_session)
        return;
    _session->close(webView);
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
    if (!_session)
        return;
    _session->close(webView);
}

@end

namespace WebKit {

Ref<SOAuthorizationSession> PopUpSOAuthorizationSession::create(SOAuthorization *soAuthorization, WebPageProxy& page, Ref<API::NavigationAction>&& navigationAction, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
{
    return adoptRef(*new PopUpSOAuthorizationSession(soAuthorization, page, WTFMove(navigationAction), WTFMove(newPageCallback), WTFMove(uiClientCallback)));
}

PopUpSOAuthorizationSession::PopUpSOAuthorizationSession(SOAuthorization *soAuthorization, WebPageProxy& page, Ref<API::NavigationAction>&& navigationAction, NewPageCallback&& newPageCallback, UIClientCallback&& uiClientCallback)
    : SOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, InitiatingAction::PopUp)
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
    ASSERT(page() && page()->isInWindow());
    start();
}

void PopUpSOAuthorizationSession::fallBackToWebPathInternal()
{
    m_uiClientCallback(releaseNavigationAction(), WTFMove(m_newPageCallback));
}

void PopUpSOAuthorizationSession::abortInternal()
{
    if (!page()) {
        m_newPageCallback(nullptr);
        return;
    }

    initSecretWebView();
    m_newPageCallback(m_secretWebView->_page.get());
    [m_secretWebView evaluateJavaScript: @"window.close()" completionHandler:nil];
}

void PopUpSOAuthorizationSession::completeInternal(const WebCore::ResourceResponse& response, NSData *data)
{
    if (response.httpStatusCode() != 200 || !page()) {
        fallBackToWebPathInternal();
        return;
    }

    initSecretWebView();
    m_newPageCallback(m_secretWebView->_page.get());
    [m_secretWebView loadData:data MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:response.url()];
}

void PopUpSOAuthorizationSession::close(WKWebView *webView)
{
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
    ASSERT(page());
    auto initiatorWebView = fromWebPageProxy(*page());
    auto configuration = adoptNS([initiatorWebView.configuration copy]);
    [configuration _setRelatedWebView:initiatorWebView];
    m_secretWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    m_secretDelegate = adoptNS([[WKSOSecretDelegate alloc] initWithSession:this]);
    [m_secretWebView setUIDelegate:m_secretDelegate.get()];
    [m_secretWebView setNavigationDelegate:m_secretDelegate.get()];

    m_secretWebView->_page->setShouldSuppressSOAuthorizationInAllNavigationPolicyDecision();
    WTFLogAlways("SecretWebView is created.");
}

} // namespace WebKit

#endif
