/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#import "SubFrameSOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "APIFrameHandle.h"
#import "APINavigationAction.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/ContentSecurityPolicy.h>
#import <WebCore/HTTPParsers.h>
#import <WebCore/HTTPStatusCodes.h>
#import <WebCore/ResourceResponse.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

#define AUTHORIZATIONSESSION_RELEASE_LOG(fmt, ...) RELEASE_LOG(AppSSO, "%p - [InitiatingAction=%s][State=%s] SubFrameSOAuthorizationSession::" fmt, this, initiatingActionString().characters(), stateString().characters(), ##__VA_ARGS__)

namespace {

constexpr auto soAuthorizationPostDidStartMessageToParent = "<script>parent.postMessage('SOAuthorizationDidStart', '*');</script>"_s;
constexpr auto soAuthorizationPostDidCancelMessageToParent = "<script>parent.postMessage('SOAuthorizationDidCancel', '*');</script>"_s;
constexpr auto soAuthorizationPostDidUserCancelMessageToParent = "<script>parent.postMessage('SOAuthorizationDidUserCancel', '*');</script>"_s;

} // namespace

Ref<SOAuthorizationSession> SubFrameSOAuthorizationSession::create(RetainPtr<WKSOAuthorizationDelegate> delegate, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler, std::optional<FrameIdentifier> frameID)
{
    return adoptRef(*new SubFrameSOAuthorizationSession(delegate, WTF::move(navigationAction), page, WTF::move(completionHandler), frameID));
}

SubFrameSOAuthorizationSession::SubFrameSOAuthorizationSession(RetainPtr<WKSOAuthorizationDelegate> delegate, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler, std::optional<FrameIdentifier> frameID)
    : NavigationSOAuthorizationSession(delegate, WTF::move(navigationAction), page, InitiatingAction::SubFrame, WTF::move(completionHandler))
    , m_frameID(frameID)
{
    if (RefPtr frame = WebFrameProxy::webFrame(m_frameID))
        frame->frameLoadState().addObserver(*this);
}

SubFrameSOAuthorizationSession::~SubFrameSOAuthorizationSession()
{
    if (RefPtr frame = WebFrameProxy::webFrame(m_frameID))
        frame->frameLoadState().removeObserver(*this);
}

void SubFrameSOAuthorizationSession::fallBackToWebPathInternal()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("fallBackToWebPathInternal: navigationAction=%p", navigationAction());
    ASSERT(navigationAction());
    appendRequestToLoad(URL(navigationAction()->request().url()), Vector<uint8_t>(soAuthorizationPostDidCancelMessageToParent.span8()));
    appendRequestToLoad(URL(navigationAction()->request().url()), String(navigationAction()->request().httpReferrer()));
}

void SubFrameSOAuthorizationSession::userCancel()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("userCancel: navigationAction=%p", navigationAction());
    ASSERT(navigationAction());
    appendRequestToLoad(URL(navigationAction()->request().url()), Vector<uint8_t>(soAuthorizationPostDidUserCancelMessageToParent.span8()));
    appendRequestToLoad(URL(navigationAction()->request().url()), String(navigationAction()->request().httpReferrer()));
}

void SubFrameSOAuthorizationSession::abortInternal()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("abortInternal");
    fallBackToWebPathInternal();
}

void SubFrameSOAuthorizationSession::completeInternal(const WebCore::ResourceResponse& response, NSData *data)
{
    AUTHORIZATIONSESSION_RELEASE_LOG("completeInternal: httpState=%d", response.httpStatusCode());
    if (response.httpStatusCode() != httpStatus200OK) {
        fallBackToWebPathInternal();
        return;
    }
    appendRequestToLoad(URL(response.url()), makeVector(data));
}

void SubFrameSOAuthorizationSession::beforeStart()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("beforeStart");
    // Cancelled the current load before loading the data to post SOAuthorizationDidStart to the parent frame.
    invokeCallback(true);
    ASSERT(navigationAction());
    appendRequestToLoad(URL(navigationAction()->request().url()), Vector<uint8_t>(soAuthorizationPostDidStartMessageToParent.span8()));
}

void SubFrameSOAuthorizationSession::didFinishLoad(IsMainFrame, const URL&)
{
    AUTHORIZATIONSESSION_RELEASE_LOG("didFinishLoad");
    RefPtr frame = WebFrameProxy::webFrame(m_frameID);
    ASSERT(frame);
    if (m_requestsToLoad.isEmpty() || m_requestsToLoad.first().first != frame->url())
        return;
    m_requestsToLoad.takeFirst();
    loadRequestToFrame();
}

void SubFrameSOAuthorizationSession::appendRequestToLoad(URL&& url, Supplement&& supplement)
{
    m_requestsToLoad.append({ WTF::move(url), WTF::move(supplement) });
    if (m_requestsToLoad.size() == 1)
        loadRequestToFrame();
}

void SubFrameSOAuthorizationSession::loadRequestToFrame()
{
    AUTHORIZATIONSESSION_RELEASE_LOG("loadRequestToFrame");
    RefPtr page = this->page();
    if (!page || m_requestsToLoad.isEmpty())
        return;

    if (RefPtr frame = WebFrameProxy::webFrame(m_frameID)) {
        page->setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision();
        auto& url = m_requestsToLoad.first().first;
        WTF::switchOn(m_requestsToLoad.first().second, [&](const Vector<uint8_t>& data) {
            frame->loadData(data, "text/html"_s, "UTF-8"_s, url);
        }, [&](const String& referrer) {
            frame->loadURL(url, referrer);
        });
    }
}

bool SubFrameSOAuthorizationSession::shouldInterruptLoadForXFrameOptions(Vector<Ref<SecurityOrigin>>&& frameAncestorOrigins, const String& xFrameOptions, const URL& url)
{
    switch (parseXFrameOptionsHeader(xFrameOptions)) {
    case XFrameOptionsDisposition::None:
    case XFrameOptionsDisposition::AllowAll:
        return false;
    case XFrameOptionsDisposition::Deny:
        return true;
    case XFrameOptionsDisposition::SameOrigin: {
        auto origin = SecurityOrigin::create(url);
        for (auto& ancestorOrigin : frameAncestorOrigins) {
            if (!origin->isSameSchemeHostPort(ancestorOrigin))
                return true;
        }
        return false;
    }
    case XFrameOptionsDisposition::Conflict: {
        auto errorMessage = makeString("Multiple 'X-Frame-Options' headers with conflicting values ('"_s, xFrameOptions, "') encountered. Falling back to 'DENY'."_s);
        AUTHORIZATIONSESSION_RELEASE_LOG("shouldInterruptLoadForXFrameOptions: %s", errorMessage.utf8().data());
        return true;
    }
    case XFrameOptionsDisposition::Invalid: {
        auto errorMessage = makeString("Invalid 'X-Frame-Options' header encountered: '"_s, xFrameOptions, "' is not a recognized directive. The header will be ignored."_s);
        AUTHORIZATIONSESSION_RELEASE_LOG("shouldInterruptLoadForXFrameOptions: %s", errorMessage.utf8().data());
        return false;
    }
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool SubFrameSOAuthorizationSession::shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(const WebCore::ResourceResponse& response)
{
    if (RefPtr page = this->page(); page && protect(page->preferences())->ignoreIframeEmbeddingProtectionsEnabled())
        return false;

    Vector<Ref<SecurityOrigin>> frameAncestorOrigins;

    ASSERT(navigationAction());
    if (RefPtr targetFrame = navigationAction()->targetFrame()) {
        if (auto parentFrameHandle = targetFrame->parentFrameHandle()) {
            for (RefPtr parent = WebFrameProxy::webFrame(parentFrameHandle->frameID()); parent; parent = parent->parentFrame())
                frameAncestorOrigins.append(SecurityOrigin::create(parent->url()));
        }
    }

    auto url = response.url();
    ContentSecurityPolicy contentSecurityPolicy { URL { url }, nullptr, nullptr };
    contentSecurityPolicy.didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, navigationAction()->request().httpReferrer());
    if (!contentSecurityPolicy.allowFrameAncestors(frameAncestorOrigins, url))
        return true;

    if (!contentSecurityPolicy.overridesXFrameOptions()) {
        String xFrameOptions = response.httpHeaderField(HTTPHeaderName::XFrameOptions);
        if (!xFrameOptions.isNull() && shouldInterruptLoadForXFrameOptions(WTF::move(frameAncestorOrigins), xFrameOptions, response.url())) {
            String errorMessage = makeString("Refused to display '"_s, response.url().stringCenterEllipsizedToLength(), "' in a frame because it set 'X-Frame-Options' to '"_s, xFrameOptions, "'."_s);
            AUTHORIZATIONSESSION_RELEASE_LOG("shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions: %s", errorMessage.utf8().data());

            return true;
        }
    }

    return false;
}

} // namespace WebKit

#undef AUTHORIZATIONSESSION_RELEASE_LOG

#endif
