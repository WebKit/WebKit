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
#import "SOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "APIHTTPCookieStore.h"
#import "APINavigation.h"
#import "APINavigationAction.h"
#import "APIUIClient.h"
#import "Logging.h"
#import "SOAuthorizationLoadPolicy.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "WebsiteDataStore.h"
#import <WebCore/ResourceResponse.h>
#import <WebCore/SecurityOrigin.h>
#import <pal/cocoa/AppSSOSoftLink.h>
#import <wtf/BlockPtr.h>
#import <wtf/Vector.h>

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_page && m_page->isAlwaysOnLoggingAllowed(), AppSSO, "%p - [InitiatingAction=%s] SOAuthorizationSession::" fmt, this, toString(m_action), ##__VA_ARGS__)

namespace WebKit {

namespace {

static const char* Redirect = "Redirect";
static const char* PopUp = "PopUp";
static const char* SubFrame = "SubFrame";

static const char* toString(const SOAuthorizationSession::InitiatingAction& action)
{
    switch (action) {
    case SOAuthorizationSession::InitiatingAction::Redirect:
        return Redirect;
    case SOAuthorizationSession::InitiatingAction::PopUp:
        return PopUp;
    case SOAuthorizationSession::InitiatingAction::SubFrame:
        return SubFrame;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

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
    RELEASE_LOG_IF_ALLOWED("shouldStart:");

    ASSERT(m_state == State::Idle);
    if (!m_page)
        return;
    shouldStartInternal();
}

void SOAuthorizationSession::start()
{
    RELEASE_LOG_IF_ALLOWED("start:");

    ASSERT((m_state == State::Idle || m_state == State::Waiting) && m_navigationAction);
    m_state = State::Active;
    [m_soAuthorization getAuthorizationHintsWithURL:m_navigationAction->request().url() responseCode:0 completion:makeBlockPtr([this, weakThis = makeWeakPtr(*this)] (SOAuthorizationHints *authorizationHints, NSError *error) {
        RELEASE_LOG_IF_ALLOWED("start: Receive SOAuthorizationHints (error=%ld)", error ? error.code : 0);

        if (!weakThis || error || !authorizationHints)
            return;
        continueStartAfterGetAuthorizationHints(authorizationHints.localizedExtensionBundleDisplayName);
    }).get()];
}

void SOAuthorizationSession::continueStartAfterGetAuthorizationHints(const String& hints)
{
    RELEASE_LOG_IF_ALLOWED("continueStartAfterGetAuthorizationHints: (hints=%s)", hints.utf8().data());

    ASSERT(m_state == State::Active);
    if (!m_page)
        return;

    m_page->decidePolicyForSOAuthorizationLoad(hints, [this, weakThis = makeWeakPtr(*this)] (SOAuthorizationLoadPolicy policy) {
        if (!weakThis)
            return;
        continueStartAfterDecidePolicy(policy);
    });
}

void SOAuthorizationSession::continueStartAfterDecidePolicy(const SOAuthorizationLoadPolicy& policy)
{
    if (policy == SOAuthorizationLoadPolicy::Ignore) {
        RELEASE_LOG_IF_ALLOWED("continueStartAfterDecidePolicy: Receive SOAuthorizationLoadPolicy::Ignore");

        fallBackToWebPath();
        return;
    }

    RELEASE_LOG_IF_ALLOWED("continueStartAfterDecidePolicy: Receive SOAuthorizationLoadPolicy::Allow");

    if (!m_soAuthorization || !m_page || !m_navigationAction)
        return;

    auto initiatorOrigin = emptyString();
    if (m_navigationAction->sourceFrame())
        initiatorOrigin = m_navigationAction->sourceFrame()->securityOrigin().securityOrigin()->toString();
    if (m_action == InitiatingAction::SubFrame && m_page->mainFrame())
        initiatorOrigin = WebCore::SecurityOrigin::create(m_page->mainFrame()->url())->toString();
    NSDictionary *authorizationOptions = @{
        SOAuthorizationOptionUserActionInitiated: @(m_navigationAction->isProcessingUserGesture()),
        SOAuthorizationOptionInitiatorOrigin: (NSString *)initiatorOrigin,
        SOAuthorizationOptionInitiatingAction: @(static_cast<NSInteger>(m_action))
    };
    [m_soAuthorization setAuthorizationOptions:authorizationOptions];

#if PLATFORM(IOS)
    if (![fromWebPageProxy(*m_page).UIDelegate respondsToSelector:@selector(_presentingViewControllerForWebView:)])
        [m_soAuthorization setEnableEmbeddedAuthorizationViewController:NO];
#endif

    auto *nsRequest = m_navigationAction->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
    [m_soAuthorization beginAuthorizationWithURL:nsRequest.URL httpHeaders:nsRequest.allHTTPHeaderFields httpBody:nsRequest.HTTPBody];
}

void SOAuthorizationSession::fallBackToWebPath()
{
    RELEASE_LOG_IF_ALLOWED("fallBackToWebPath:");

    if (m_state != State::Active)
        return;
    becomeCompleted();
    fallBackToWebPathInternal();
}

void SOAuthorizationSession::abort()
{
    RELEASE_LOG_IF_ALLOWED("abort:");

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

    RELEASE_LOG_IF_ALLOWED("complete: (httpStatusCode=%d, hasCookies=%d, hasData=%d)", response.httpStatusCode(), !cookies.isEmpty(), !!data.length);

    if (cookies.isEmpty()) {
        completeInternal(response, data);
        return;
    }

    if (!m_page)
        return;
    m_page->websiteDataStore().cookieStore().setCookies(cookies, [this, weakThis = makeWeakPtr(*this), response = WTFMove(response), data = adoptNS([[NSData alloc] initWithData:data])] () mutable {
        if (!weakThis)
            return;

        RELEASE_LOG_IF_ALLOWED("complete: Cookies are set.");

        completeInternal(response, data.get());
    });
}

void SOAuthorizationSession::presentViewController(SOAuthorizationViewController viewController, UICallback uiCallback)
{
    RELEASE_LOG_IF_ALLOWED("presentViewController:");

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
    RELEASE_LOG_IF_ALLOWED("dismissViewController:");

    ASSERT(m_viewController);
#if PLATFORM(MAC)
    ASSERT(m_sheetWindow && m_sheetWindowWillCloseObserver);

    // This is a workaround for an AppKit issue: <rdar://problem/59125329>.
    // [m_sheetWindow sheetParent] is null if the parent is minimized or the host app is hidden.
    if (m_page && m_page->platformWindow()) {
        auto *presentingWindow = m_page->platformWindow();
        if (presentingWindow.miniaturized) {
            if (m_presentingWindowDidDeminiaturizeObserver)
                return;
            m_presentingWindowDidDeminiaturizeObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidDeminiaturizeNotification object:presentingWindow queue:nil usingBlock:[protectedThis = makeRefPtr(this), this] (NSNotification *) {
                dismissViewController();
                [[NSNotificationCenter defaultCenter] removeObserver:m_presentingWindowDidDeminiaturizeObserver.get()];
                m_presentingWindowDidDeminiaturizeObserver = nullptr;
            }];
            return;
        }
    }

    if (NSApp.hidden) {
        if (m_applicationDidUnhideObserver)
            return;
        m_applicationDidUnhideObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidUnhideNotification object:NSApp queue:nil usingBlock:[protectedThis = makeRefPtr(this), this] (NSNotification *) {
            dismissViewController();
            [[NSNotificationCenter defaultCenter] removeObserver:m_applicationDidUnhideObserver.get()];
            m_applicationDidUnhideObserver = nullptr;
        }];
        return;
    }

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

#undef RELEASE_LOG_IF_ALLOWED

#endif // HAVE(APP_SSO)
