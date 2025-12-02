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
#import "SOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "APIFrameHandle.h"
#import "APIHTTPCookieStore.h"
#import "APINavigation.h"
#import "APINavigationAction.h"
#import "APIUIClient.h"
#import "Logging.h"
#import "SOAuthorizationLoadPolicy.h"
#import "UIKitUtilities.h"
#import "WKSOAuthorizationDelegate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import "WebsiteDataStore.h"
#import <WebCore/ContentSecurityPolicy.h>
#import <WebCore/HTTPParsers.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/BlockPtr.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#import <pal/cocoa/AppSSOSoftLink.h>

#define AUTHORIZATIONSESSION_RELEASE_LOG(fmt, ...) RELEASE_LOG(AppSSO, "%p - [InitiatingAction=%s][State=%s] SOAuthorizationSession::" fmt, this, toString(m_action).characters(), stateString().characters(), ##__VA_ARGS__)
#define AUTHORIZATIONSESSION_RELEASE_LOG_WITH_THIS(thisPtr, fmt, ...) RELEASE_LOG(AppSSO, "%p - [InitiatingAction=%s][State=%s] SOAuthorizationSession::" fmt, thisPtr.get(), toString(thisPtr->m_action).characters(), thisPtr->stateString().characters(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

namespace {

static constexpr auto Redirect = "Redirect"_s;
static constexpr auto PopUp = "PopUp"_s;
static constexpr auto SubFrame = "SubFrame"_s;

static ASCIILiteral toString(const SOAuthorizationSession::InitiatingAction& action)
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
    return { };
}

static Vector<WebCore::Cookie> toCookieVector(NSArray<NSHTTPCookie *> *cookies)
{
    return Vector<WebCore::Cookie>(cookies.count, [cookies](size_t i) {
        return WebCore::Cookie { cookies[i] };
    });
}

static bool isSameOrigin(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
{
    auto requestOrigin = WebCore::SecurityOrigin::create(request.url());
    return requestOrigin->isSameOriginAs(WebCore::SecurityOrigin::create(response.url()).get());
}

} // namespace

SOAuthorizationSession::SOAuthorizationSession(RetainPtr<WKSOAuthorizationDelegate> delegate, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, InitiatingAction action)
    : m_soAuthorization(adoptNS([PAL::allocSOAuthorizationInstance() init]))
    , m_navigationAction(WTFMove(navigationAction))
    , m_page(page)
    , m_action(action)
{
    m_soAuthorization.get().delegate = delegate.get();
}

SOAuthorizationSession::~SOAuthorizationSession()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("~SOAuthorizationSession: m_viewController=%p", m_viewController.get());
#if PLATFORM(MAC)
    AUTHORIZATIONSESSION_RELEASE_LOG("~SOAuthorizationSession: m_sheetWindow=%p", m_sheetWindow.get());
#endif
    m_isInDestructor = true;

    if (m_state == State::Active && !!m_soAuthorization)
        [m_soAuthorization cancelAuthorization];
    if (m_state != State::Idle && m_state != State::Completed)
        becomeCompleted();
    else
        dismissViewController();
}

ASCIILiteral SOAuthorizationSession::initiatingActionString() const
{
    return toString(m_action);
}

ASCIILiteral SOAuthorizationSession::stateString() const
{
    static constexpr auto Idle = "Idle"_s;
    static constexpr auto Active = "Active"_s;
    static constexpr auto Waiting = "Waiting"_s;
    static constexpr auto Completed = "Completed"_s;

    switch (m_state) {
    case State::Idle:
        return Idle;
    case State::Active:
        return Active;
    case State::Waiting:
        return Waiting;
    case State::Completed:
        return Completed;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}


Ref<API::NavigationAction> SOAuthorizationSession::releaseNavigationAction()
{
    return m_navigationAction.releaseNonNull();
}

void SOAuthorizationSession::becomeCompleted()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("becomeCompleted: m_viewController=%p", m_viewController.get());

    ASSERT(m_state == State::Active || m_state == State::Waiting);
    m_state = State::Completed;
    dismissViewController();
#if PLATFORM(MAC)
    ASSERT(!m_sheetWindow || (m_sheetWindow && m_sheetWindowWillCloseObserver));
#endif
}

void SOAuthorizationSession::shouldStart()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("shouldStart: m_page=%p", page());

    ASSERT(m_state == State::Idle);
    if (!m_page)
        return;
    shouldStartInternal();
}

void SOAuthorizationSession::start()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("start: navigationAction=%p", navigationAction());

    ASSERT((m_state == State::Idle || m_state == State::Waiting) && m_navigationAction);
    m_state = State::Active;
    AUTHORIZATIONSESSION_RELEASE_LOG("start: Moving m_state to Active.");
    [m_soAuthorization getAuthorizationHintsWithURL:m_navigationAction->request().url().createNSURL().get() responseCode:0 completion:makeBlockPtr([weakThis = ThreadSafeWeakPtr { *this }] (SOAuthorizationHints *authorizationHints, NSError *error) {
        auto protectedThis = weakThis.get();
        if (!protectedThis) {
            RELEASE_LOG_ERROR(AppSSO, "SOAuthorizationSession::start (getAuthorizationHintsWithURL completion handler): Returning early because weakThis is now null.");
            return;
        }

        AUTHORIZATIONSESSION_RELEASE_LOG_WITH_THIS(protectedThis, "start: Receive SOAuthorizationHints (error=%ld)", error ? error.code : 0);

        if (error || !authorizationHints) {
            AUTHORIZATIONSESSION_RELEASE_LOG_WITH_THIS(protectedThis, "start (getAuthorizationHintsWithURL completion handler): Returning early due to error or lack of hints.");
            return;
        }

        AUTHORIZATIONSESSION_RELEASE_LOG_WITH_THIS(protectedThis, "start (getAuthorizationHintsWithURL completion handler): Receive SOAuthorizationHints.");
        protectedThis->continueStartAfterGetAuthorizationHints(authorizationHints.localizedExtensionBundleDisplayName);
    }).get()];
}

void SOAuthorizationSession::continueStartAfterGetAuthorizationHints(const String& hints)
{
    AUTHORIZATIONSESSION_RELEASE_LOG("continueStartAfterGetAuthorizationHints: (hints=%s)", hints.utf8().data());

    ASSERT(m_state == State::Active);
    RefPtr page = m_page.get();
    if (!page) {
        AUTHORIZATIONSESSION_RELEASE_LOG("continueStartAfterGetAuthorizationHints: Early return due to null m_page");
        return;
    }

    AUTHORIZATIONSESSION_RELEASE_LOG("continueStartAfterGetAuthorizationHints: Checking page for policy choice.");
    page->decidePolicyForSOAuthorizationLoad(hints, [weakThis = ThreadSafeWeakPtr { *this }] (SOAuthorizationLoadPolicy policy) {
        if (auto protectedThis = weakThis.get())
            protectedThis->continueStartAfterDecidePolicy(policy);
    });
}

void SOAuthorizationSession::continueStartAfterDecidePolicy(const SOAuthorizationLoadPolicy& policy)
{
    if (policy == SOAuthorizationLoadPolicy::Ignore) {
        AUTHORIZATIONSESSION_RELEASE_LOG("continueStartAfterDecidePolicy: Receive SOAuthorizationLoadPolicy::Ignore. Falling back to web path.");
        fallBackToWebPath();
        return;
    }

    AUTHORIZATIONSESSION_RELEASE_LOG("continueStartAfterDecidePolicy: Receive SOAuthorizationLoadPolicy::Allow");

    if (!m_soAuthorization || !m_page || !m_navigationAction) {
        AUTHORIZATIONSESSION_RELEASE_LOG("continueStartAfterGetAuthorizationHints: Early return m_soAuthorization=%d, m_page=%p, navigationAction=%p.", !!m_soAuthorization, page(), navigationAction());
        return;
    }

    auto initiatorOrigin = emptyString();
    if (RefPtr sourceOrigin = m_navigationAction->sourceFrame() ? m_navigationAction->sourceFrame()->securityOrigin().securityOrigin().ptr() : nullptr; sourceOrigin && !sourceOrigin->isOpaque())
        initiatorOrigin = sourceOrigin->toString();
    if (m_action == InitiatingAction::SubFrame && m_page->mainFrame())
        initiatorOrigin = WebCore::SecurityOrigin::create(m_page->mainFrame()->url())->toString();
    RetainPtr<NSDictionary> authorizationOptions = @{
        SOAuthorizationOptionUserActionInitiated: @(m_navigationAction->isProcessingUserGesture()),
        SOAuthorizationOptionInitiatorOrigin: initiatorOrigin.createNSString().get(),
        SOAuthorizationOptionInitiatingAction: @(static_cast<NSInteger>(m_action)),
        kSOAuthorizationOptionInitiatingPath: m_page->mainFrame()->url().path().createNSString().get()
    };
#if PLATFORM(IOS_FAMILY)
    RetainPtr<WKWebView> webView = m_page->cocoaView();
    id webViewUIDelegate = [webView UIDelegate];
    if ([webViewUIDelegate respondsToSelector:@selector(_hostSceneIdentifierForWebView:)]) {
        NSString *callerSceneID = [webViewUIDelegate _hostSceneIdentifierForWebView:webView.get()];
        if (callerSceneID) {
            RetainPtr mutableAuthorizationOptions = adoptNS([authorizationOptions mutableCopy]);
            mutableAuthorizationOptions.get()[@"callerSceneIdentifier"] = callerSceneID;
            authorizationOptions = WTFMove(mutableAuthorizationOptions);
        }
    }
#endif
    [m_soAuthorization setAuthorizationOptions:authorizationOptions.get()];

#if PLATFORM(VISION)
    // rdar://130904577 - Investigate supporting embedded authorization view controller on visionOS.
    if (![[m_page->cocoaView() UIDelegate] respondsToSelector:@selector(_presentingViewControllerForWebView:)] || WTF::IOSApplication::isSafariViewService())
        [m_soAuthorization setEnableEmbeddedAuthorizationViewController:NO];
#endif

    RetainPtr nsRequest = m_navigationAction->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);
    AUTHORIZATIONSESSION_RELEASE_LOG("continueStartAfterGetAuthorizationHints: Beginning authorization with AppSSO.");
    [m_soAuthorization beginAuthorizationWithURL:retainPtr(nsRequest.get().URL).get() httpHeaders:retainPtr(nsRequest.get().allHTTPHeaderFields).get() httpBody:retainPtr(nsRequest.get().HTTPBody).get()];
}

void SOAuthorizationSession::fallBackToWebPath()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("fallBackToWebPath");

    if (m_state != State::Active) {
        AUTHORIZATIONSESSION_RELEASE_LOG("fallBackToWebPath: Returning early since not active.");
        dismissViewController();
        return;
    }

    becomeCompleted();
    fallBackToWebPathInternal();
}

void SOAuthorizationSession::abort()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("abort: m_viewController=%p", m_viewController.get());
#if PLATFORM(MAC)
    AUTHORIZATIONSESSION_RELEASE_LOG("abort: m_sheetWindow=%p", m_sheetWindow.get());
#endif

    if (m_state == State::Idle || m_state == State::Completed) {
        AUTHORIZATIONSESSION_RELEASE_LOG("abort: Returning early since idle or already completed.");
        dismissViewController();
        return;
    }

    becomeCompleted();
    abortInternal();
}

void SOAuthorizationSession::complete(NSHTTPURLResponse *httpResponse, NSData *data)
{
    AUTHORIZATIONSESSION_RELEASE_LOG("complete: httpState=%d, m_viewController=%p", static_cast<int>(httpResponse.statusCode), m_viewController.get());
#if PLATFORM(MAC)
    AUTHORIZATIONSESSION_RELEASE_LOG("complete: m_sheetWindow=%p", m_sheetWindow.get());
#endif

    if (m_state != State::Active) {
        AUTHORIZATIONSESSION_RELEASE_LOG("complete: Returning early since not active.");
        dismissViewController();
        return;
    }

    ASSERT(m_navigationAction);
    becomeCompleted();

    auto response = WebCore::ResourceResponse(httpResponse);

    if (shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(response)) {
        AUTHORIZATIONSESSION_RELEASE_LOG("complete: CSP failed. Falling back to web path.");
        fallBackToWebPathInternal();
        return;
    }

    if (!isSameOrigin(m_navigationAction->request(), response)) {
        AUTHORIZATIONSESSION_RELEASE_LOG("complete:  Origins don't match. Falling back to web path.");
        fallBackToWebPathInternal();
        return;
    }

    // Set cookies.
    auto cookies = toCookieVector([NSHTTPCookie cookiesWithResponseHeaderFields:retainPtr(httpResponse.allHeaderFields).get() forURL:response.url().createNSURL().get()]);

    AUTHORIZATIONSESSION_RELEASE_LOG("complete: (httpStatusCode=%d, hasCookies=%d, hasData=%d)", response.httpStatusCode(), !cookies.isEmpty(), !!data.length);

    if (cookies.isEmpty()) {
        AUTHORIZATIONSESSION_RELEASE_LOG("complete:  No cookies to set. Completing (internal).");
        completeInternal(response, data);
        return;
    }

    RefPtr page = m_page.get();
    if (!page) {
        AUTHORIZATIONSESSION_RELEASE_LOG("complete:  Returning early because m_page is null.");
        return;
    }

    // https://bugs.webkit.org/show_bug.cgi?id=299083
    if (!cookies.isEmpty()) {
        size_t totalHeaderSize = 0;
        for (const auto& cookie : cookies)
            totalHeaderSize += cookie.name.length() + cookie.value.length() + 1;
        AUTHORIZATIONSESSION_RELEASE_LOG("complete: Setting %zu cookies with total header size ~%zu bytes in data store %p", cookies.size(), totalHeaderSize, page->protectedWebsiteDataStore().ptr());
    }

    page->protectedWebsiteDataStore()->protectedCookieStore()->setCookies(WTFMove(cookies), [weakThis = ThreadSafeWeakPtr { *this }, response = WTFMove(response), data = adoptNS([[NSData alloc] initWithData:data])] () mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        AUTHORIZATIONSESSION_RELEASE_LOG_WITH_THIS(protectedThis, "complete: Cookies are set.");

        protectedThis->completeInternal(response, data.get());
    });
}

void SOAuthorizationSession::presentViewController(SOAuthorizationViewController viewController, UICallback uiCallback)
{
    AUTHORIZATIONSESSION_RELEASE_LOG("presentViewController: m_viewController=%p", m_viewController.get());
#if PLATFORM(MAC)
    AUTHORIZATIONSESSION_RELEASE_LOG("presentViewController: m_sheetWindow=%p", m_sheetWindow.get());
#endif

    ASSERT(m_state == State::Active);
    // Only expect at most one UI session for the whole authorization session.
    RefPtr page = m_page.get();
    if (!page || page->isClosed() || m_viewController) {
        AUTHORIZATIONSESSION_RELEASE_LOG("presentViewController: m_page=%p, m_page->isClosed=%d, m_viewController=%p", page.get(), page ? page->isClosed() : 0, m_viewController.get());
        uiCallback(NO, adoptNS([[NSError alloc] initWithDomain:SOErrorDomain code:kSOErrorAuthorizationPresentationFailed userInfo:nil]).get());
        return;
    }

    m_viewController = viewController;
#if PLATFORM(MAC)
    ASSERT(!m_sheetWindow);
    ASSERT(!m_sheetWindowWillCloseObserver);

    dismissModalSheetIfNecessary();

    m_sheetWindow = [NSWindow windowWithContentViewController:m_viewController.get()];

    m_sheetWindowWillCloseObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification object:m_sheetWindow.get() queue:nil usingBlock:[weakThis = ThreadSafeWeakPtr { *this }] (NSNotification *) {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        RELEASE_LOG(AppSSO, "presentViewController: Received NSWindowWillCloseNotification. Dismissing the view controller.");
        protectedThis->dismissViewController();
    }];
    AUTHORIZATIONSESSION_RELEASE_LOG("presentViewController: Added m_sheetWindowWillCloseObserver (%p)", m_sheetWindowWillCloseObserver.get());

    RetainPtr presentingWindow = page->platformWindow();
    if (!presentingWindow) {
        AUTHORIZATIONSESSION_RELEASE_LOG("presentViewController: No presenting window. Returning early.");
        uiCallback(NO, adoptNS([[NSError alloc] initWithDomain:SOErrorDomain code:kSOErrorAuthorizationPresentationFailed userInfo:nil]).get());
        return;
    }

    AUTHORIZATIONSESSION_RELEASE_LOG("presentViewController: Calling beginSheet on %p for sheet %p.", presentingWindow.get(), m_sheetWindow.get());
    [presentingWindow beginSheet:m_sheetWindow.get() completionHandler:nil];
#elif PLATFORM(IOS) || PLATFORM(VISION)
    // FIXME: When in element fullscreen, UIClient::presentingViewController() may not return the
    // WKFullScreenViewController even though that is the presenting view controller of the WKWebView.
    // We should call PageClientImpl::presentingViewController() instead.
    UIViewController *presentingViewController = page->uiClient().presentingViewController();
#if !PLATFORM(VISION)
    if (!presentingViewController)
        presentingViewController = [page->cocoaView() _wk_viewControllerForFullScreenPresentation];
#endif
    if (!presentingViewController) {
        uiCallback(NO, adoptNS([[NSError alloc] initWithDomain:SOErrorDomain code:kSOErrorAuthorizationPresentationFailed userInfo:nil]).get());
        return;
    }

    [presentingViewController presentViewController:m_viewController.get() animated:YES completion:nil];
#endif

    uiCallback(YES, nil);
}

#if PLATFORM(MAC)
void SOAuthorizationSession::dismissModalSheetIfNecessary()
{
    if (RetainPtr<NSWindow> presentingWindow = m_sheetWindow.get().sheetParent) {
        AUTHORIZATIONSESSION_RELEASE_LOG("dismissModalSheetIfNecessary: Calling endSheet on %p for sheet %p.", presentingWindow.get(), m_sheetWindow.get());
        [presentingWindow endSheet:m_sheetWindow.get()];
    }
    m_sheetWindow = nullptr;

    if (m_sheetWindowWillCloseObserver.get()) {
        AUTHORIZATIONSESSION_RELEASE_LOG("dismissModalSheetIfNecessary: Removing m_sheetWindowWillCloseObserver (%p)", m_sheetWindowWillCloseObserver.get());
        [[NSNotificationCenter defaultCenter] removeObserver:m_sheetWindowWillCloseObserver.get()];
    }
    m_sheetWindowWillCloseObserver = nullptr;
}
#endif

void SOAuthorizationSession::dismissViewController()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: m_viewController=%p", m_viewController.get());
    if (!m_viewController) {
        AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: No view controller, so returning early.");
        return;
    }

#if PLATFORM(MAC)
    if (!m_sheetWindow) {
        ASSERT(!m_sheetWindowWillCloseObserver);
        AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: No view controller or sheet window, so returning early.");
        return;
    }

    ASSERT(m_sheetWindowWillCloseObserver);

    // This is a workaround for an AppKit issue: <rdar://problem/59125329>.
    // [m_sheetWindow sheetParent] is null if the parent is minimized or the host app is hidden.
    if (!m_isInDestructor) {
        RefPtr page = m_page.get();
        if (page && page->platformWindow()) {
            RetainPtr presentingWindow = page->platformWindow();
            if (presentingWindow.get().miniaturized) {
                AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: Page's window is miniaturized. Waiting to dismiss until active.");
                if (m_presentingWindowDidDeminiaturizeObserver) {
                    AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: [Miniaturized] Already has a deminiaturized observer (%p). Hidden observer is %p", m_presentingWindowDidDeminiaturizeObserver.get(), m_applicationDidUnhideObserver.get());
                    return;
                }
                m_presentingWindowDidDeminiaturizeObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidDeminiaturizeNotification object:presentingWindow.get() queue:nil usingBlock:[protectedThis = Ref { *this }, this] (NSNotification *) {
                    AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: Window has deminiaturized. Completing the dismissal.");
                    dismissViewController();
                    [[NSNotificationCenter defaultCenter] removeObserver:m_presentingWindowDidDeminiaturizeObserver.get()];
                    m_presentingWindowDidDeminiaturizeObserver = nullptr;
                }];
                return;
            }
        }
    }

    // FIXME: This is a safer cpp false positive (rdar://problem/161068288).
    SUPPRESS_UNRETAINED_ARG if (!m_isInDestructor && NSApp.hidden) {
        AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: Application is hidden. Waiting to dismiss until active.");
        if (m_applicationDidUnhideObserver) {
            AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: [Hidden] Already has an Unhide observer (%p). Deminiaturized observer is %p", m_presentingWindowDidDeminiaturizeObserver.get(), m_applicationDidUnhideObserver.get());
            return;
        }
        // FIXME: We should not need to protect NSApp here (rdar://problem/161068288).
        m_applicationDidUnhideObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationDidUnhideNotification object:NSApp queue:nil usingBlock:[protectedThis = Ref { *this }, this] (NSNotification *) {
            AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: Application is no longer hidden. Completing the dismissal.");
            dismissViewController();
            [[NSNotificationCenter defaultCenter] removeObserver:m_applicationDidUnhideObserver.get()];
            m_applicationDidUnhideObserver = nullptr;
        }];
        return;
    }

    dismissModalSheetIfNecessary();
    AUTHORIZATIONSESSION_RELEASE_LOG("dismissViewController: Finished call with deminiaturized observer (%p) and Hidden observer (%p)", m_presentingWindowDidDeminiaturizeObserver.get(), m_applicationDidUnhideObserver.get());
#elif PLATFORM(IOS) || PLATFORM(VISION)
    [[m_viewController presentingViewController] dismissViewControllerAnimated:YES completion:nil];
#endif

    m_viewController = nullptr;
}

} // namespace WebKit

#undef AUTHORIZATIONSESSION_RELEASE_LOG

#endif // HAVE(APP_SSO)
