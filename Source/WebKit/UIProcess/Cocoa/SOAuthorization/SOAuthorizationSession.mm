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

#include "config.h"
#include "SOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "APIHTTPCookieStore.h"
#import "APINavigation.h"
#import "APINavigationAction.h"
#import "APIUIClient.h"
#import "SOAuthorizationLoadPolicy.h"
#import "WKUIDelegatePrivate.h"
#import "WebPageProxy.h"
#import "WebSiteDataStore.h"
#import <WebCore/ResourceResponse.h>
#import <WebCore/SecurityOrigin.h>
#import <pal/cocoa/AppSSOSoftLink.h>
#import <wtf/Vector.h>

namespace WebKit {

namespace {

static Vector<WebCore::Cookie> toCookieVector(NSArray<NSHTTPCookie *> *cookies)
{
    Vector<WebCore::Cookie> result;
    result.reserveInitialCapacity(cookies.count);
    for (id cookie in cookies)
        result.uncheckedAppend(cookie);
    return result;
}

static bool isSameOrigin(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
{
    auto requestOrigin = WebCore::SecurityOrigin::create(request.url());
    return requestOrigin->isSameOriginAs(WebCore::SecurityOrigin::create(response.url()).get());
}

} // namespace

SOAuthorizationSession::SOAuthorizationSession(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, InitiatingAction action)
    : m_soAuthorization(soAuthorization)
    , m_navigationAction(WTFMove(navigationAction))
    , m_page(makeWeakPtr(page))
    , m_action(action)
{
}

SOAuthorizationSession::~SOAuthorizationSession()
{
    if (m_state == State::Active && !!m_soAuthorization)
        [m_soAuthorization cancelAuthorization];
    if (m_state != State::Idle && m_state != State::Completed)
        becomeCompleted();
}

Ref<API::NavigationAction> SOAuthorizationSession::releaseNavigationAction()
{
    return m_navigationAction.releaseNonNull();
}

void SOAuthorizationSession::becomeCompleted()
{
    ASSERT(m_state == State::Active || m_state == State::Waiting);
    m_state = State::Completed;
    if (m_viewController)
        dismissViewController();
}

void SOAuthorizationSession::shouldStart()
{
    ASSERT(m_state == State::Idle);
    if (!m_page)
        return;
    shouldStartInternal();
}

void SOAuthorizationSession::start()
{
    ASSERT((m_state == State::Idle || m_state == State::Waiting) && m_page && m_navigationAction);
    m_state = State::Active;

    m_page->decidePolicyForSOAuthorizationLoad(emptyString(), [this, weakThis = makeWeakPtr(*this)] (SOAuthorizationLoadPolicy policy) {
        if (!weakThis)
            return;

        if (policy == SOAuthorizationLoadPolicy::Ignore) {
            fallBackToWebPath();
            return;
        }

        if (!m_soAuthorization || !m_page || !m_navigationAction)
            return;

        // FIXME: <rdar://problem/48909336> Replace the below with AppSSO constants.
        auto initiatorOrigin = emptyString();
        if (m_navigationAction->sourceFrame())
            initiatorOrigin = m_navigationAction->sourceFrame()->securityOrigin().securityOrigin().toString();
        if (m_action == InitiatingAction::SubFrame && m_page->mainFrame())
            initiatorOrigin = WebCore::SecurityOrigin::create(m_page->mainFrame()->url())->toString();
        NSDictionary *authorizationOptions = @{
            SOAuthorizationOptionUserActionInitiated: @(m_navigationAction->isProcessingUserGesture()),
            @"initiatorOrigin": (NSString *)initiatorOrigin,
            @"initiatingAction": @(static_cast<NSInteger>(m_action))
        };
        [m_soAuthorization setAuthorizationOptions:authorizationOptions];

#if PLATFORM(IOS)
        if (![fromWebPageProxy(*m_page).UIDelegate respondsToSelector:@selector(_presentingViewControllerForWebView:)])
            [m_soAuthorization setEnableEmbeddedAuthorizationViewController:NO];
#endif

        auto *nsRequest = m_navigationAction->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
        [m_soAuthorization beginAuthorizationWithURL:nsRequest.URL httpHeaders:nsRequest.allHTTPHeaderFields httpBody:nsRequest.HTTPBody];
    });
}

void SOAuthorizationSession::fallBackToWebPath()
{
    if (m_state != State::Active)
        return;
    becomeCompleted();
    fallBackToWebPathInternal();
}

void SOAuthorizationSession::abort()
{
    if (m_state == State::Idle || m_state == State::Completed)
        return;
    becomeCompleted();
    abortInternal();
}

void SOAuthorizationSession::complete(NSHTTPURLResponse *httpResponse, NSData *data)
{
    if (m_state != State::Active)
        return;
    ASSERT(m_navigationAction);
    becomeCompleted();

    auto response = WebCore::ResourceResponse(httpResponse);
    if (!isSameOrigin(m_navigationAction->request(), response)) {
        fallBackToWebPathInternal();
        return;
    }

    // Set cookies.
    auto cookies = toCookieVector([NSHTTPCookie cookiesWithResponseHeaderFields:httpResponse.allHeaderFields forURL:response.url()]);
    if (cookies.isEmpty()) {
        completeInternal(response, data);
        return;
    }

    if (!m_page)
        return;
    m_page->websiteDataStore().cookieStore().setCookies(cookies, [weakThis = makeWeakPtr(*this), response = WTFMove(response), data = adoptNS([[NSData alloc] initWithData:data])] () mutable {
        if (!weakThis)
            return;
        weakThis->completeInternal(response, data.get());
    });
}

void SOAuthorizationSession::presentViewController(SOAuthorizationViewController viewController, UICallback uiCallback)
{
    ASSERT(m_state == State::Active);
    // Only expect at most one UI session for the whole authorization session.
    if (!m_page || m_page->isClosed() || m_viewController) {
        uiCallback(NO, adoptNS([[NSError alloc] initWithDomain:SOErrorDomain code:kSOErrorAuthorizationPresentationFailed userInfo:nil]).get());
        return;
    }

    m_viewController = viewController;
#if PLATFORM(MAC)
    ASSERT(!m_sheetWindow);
    m_sheetWindow = [NSWindow windowWithContentViewController:m_viewController.get()];

    ASSERT(!m_sheetWindowWillCloseObserver);
    m_sheetWindowWillCloseObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification object:m_sheetWindow.get() queue:nil usingBlock:[weakThis = makeWeakPtr(*this)] (NSNotification *) {
        if (!weakThis)
            return;
        weakThis->dismissViewController();
    }];

    NSWindow *presentingWindow = m_page->platformWindow();
    if (!presentingWindow) {
        uiCallback(NO, adoptNS([[NSError alloc] initWithDomain:SOErrorDomain code:kSOErrorAuthorizationPresentationFailed userInfo:nil]).get());
        return;
    }
    [presentingWindow beginSheet:m_sheetWindow.get() completionHandler:nil];
#elif PLATFORM(IOS)
    UIViewController *presentingViewController = m_page->uiClient().presentingViewController();
    if (!presentingViewController) {
        uiCallback(NO, adoptNS([[NSError alloc] initWithDomain:SOErrorDomain code:kSOErrorAuthorizationPresentationFailed userInfo:nil]).get());
        return;
    }

    [presentingViewController presentViewController:m_viewController.get() animated:YES completion:nil];
#endif

    uiCallback(YES, nil);
}

void SOAuthorizationSession::dismissViewController()
{
    ASSERT(m_viewController);
#if PLATFORM(MAC)
    ASSERT(m_sheetWindow && m_sheetWindowWillCloseObserver);

    [[NSNotificationCenter defaultCenter] removeObserver:m_sheetWindowWillCloseObserver.get()];
    m_sheetWindowWillCloseObserver = nullptr;

    [[m_sheetWindow sheetParent] endSheet:m_sheetWindow.get()];
    m_sheetWindow = nullptr;
#elif PLATFORM(IOS)
    [[m_viewController presentingViewController] dismissViewControllerAnimated:YES completion:nil];
#endif

    m_viewController = nullptr;
}

} // namespace WebKit

#endif
