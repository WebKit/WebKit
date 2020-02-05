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
#import "SubFrameSOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "APINavigationAction.h"
#import "DataReference.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/ResourceResponse.h>
#import <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

namespace {

const char* soAuthorizationPostDidStartMessageToParent = "<script>parent.postMessage('SOAuthorizationDidStart', '*');</script>";
const char* soAuthorizationPostDidCancelMessageToParent = "<script>parent.postMessage('SOAuthorizationDidCancel', '*');</script>";

static inline Vector<uint8_t> convertBytesToVector(const uint8_t byteArray[], const size_t length)
{
    Vector<uint8_t> result;
    result.append(byteArray, length);
    return result;
}

} // namespace

Ref<SOAuthorizationSession> SubFrameSOAuthorizationSession::create(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler, FrameIdentifier frameID)
{
    return adoptRef(*new SubFrameSOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, WTFMove(completionHandler), frameID));
}

SubFrameSOAuthorizationSession::SubFrameSOAuthorizationSession(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler, FrameIdentifier frameID)
    : NavigationSOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, InitiatingAction::SubFrame, WTFMove(completionHandler))
    , m_frameID(frameID)
{
    if (auto* frame = page.process().webFrame(m_frameID))
        frame->frameLoadState().addObserver(*this);
}

SubFrameSOAuthorizationSession::~SubFrameSOAuthorizationSession()
{
    auto* page = this->page();
    if (!page)
        return;
    if (auto* frame = page->process().webFrame(m_frameID))
        frame->frameLoadState().removeObserver(*this);
}

void SubFrameSOAuthorizationSession::fallBackToWebPathInternal()
{
    ASSERT(navigationAction());
    appendRequestToLoad(URL(navigationAction()->request().url()), convertBytesToVector(reinterpret_cast<const uint8_t*>(soAuthorizationPostDidCancelMessageToParent), strlen(soAuthorizationPostDidCancelMessageToParent)));
    appendRequestToLoad(URL(navigationAction()->request().url()), String(navigationAction()->request().httpReferrer()));
}

void SubFrameSOAuthorizationSession::abortInternal()
{
    fallBackToWebPathInternal();
}

void SubFrameSOAuthorizationSession::completeInternal(const WebCore::ResourceResponse& response, NSData *data)
{
    if (response.httpStatusCode() != 200) {
        fallBackToWebPathInternal();
        return;
    }
    appendRequestToLoad(URL(response.url()), convertBytesToVector(reinterpret_cast<const uint8_t*>(data.bytes), data.length));
}

void SubFrameSOAuthorizationSession::beforeStart()
{
    // Cancelled the current load before loading the data to post SOAuthorizationDidStart to the parent frame.
    invokeCallback(true);
    ASSERT(navigationAction());
    appendRequestToLoad(URL(navigationAction()->request().url()), convertBytesToVector(reinterpret_cast<const uint8_t*>(soAuthorizationPostDidStartMessageToParent), strlen(soAuthorizationPostDidStartMessageToParent)));
}

void SubFrameSOAuthorizationSession::didFinishLoad()
{
    auto* page = this->page();
    if (!page)
        return;
    auto* frame = page->process().webFrame(m_frameID);
    ASSERT(frame);
    if (m_requestsToLoad.isEmpty() || m_requestsToLoad.first().first != frame->url())
        return;
    m_requestsToLoad.takeFirst();
    loadRequestToFrame();
}

void SubFrameSOAuthorizationSession::appendRequestToLoad(URL&& url, Supplement&& supplement)
{
    m_requestsToLoad.append({ WTFMove(url), WTFMove(supplement) });
    if (m_requestsToLoad.size() == 1)
        loadRequestToFrame();
}

void SubFrameSOAuthorizationSession::loadRequestToFrame()
{
    auto* page = this->page();
    if (!page || m_requestsToLoad.isEmpty())
        return;

    if (auto* frame = page->process().webFrame(m_frameID)) {
        page->setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision();
        auto& url = m_requestsToLoad.first().first;
        WTF::switchOn(m_requestsToLoad.first().second, [&](const Vector<uint8_t>& data) {
            frame->loadData(IPC::DataReference(data), "text/html", "UTF-8", url);
        }, [&](const String& referrer) {
            frame->loadURL(url, referrer);
        });
    }
}

} // namespace WebKit

#endif
