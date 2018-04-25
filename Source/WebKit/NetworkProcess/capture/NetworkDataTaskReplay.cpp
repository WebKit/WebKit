/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "NetworkDataTaskReplay.h"

#if ENABLE(NETWORK_CAPTURE)

#include "NetworkCaptureEvent.h"
#include "NetworkCaptureLogging.h"
#include "NetworkCaptureResource.h"
#include "NetworkLoadParameters.h"
#include "NetworkSession.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/RunLoop.h>

#define DEBUG_CLASS NetworkDataTaskReplay

namespace WebKit {
namespace NetworkCapture {

static const char* const webKitRelayDomain = "WebKitReplay";

NetworkDataTaskReplay::NetworkDataTaskReplay(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters, Resource* resource)
    : NetworkDataTask(session, client, parameters.request, parameters.storedCredentialsPolicy, parameters.shouldClearReferrerOnHTTPSToHTTPRedirect, parameters.isMainFrameNavigation)
    , m_currentRequest(m_firstRequest)
    , m_resource(resource)
{
    DEBUG_LOG("request URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));
    DEBUG_LOG("cached URL = " STRING_SPECIFIER, resource ? DEBUG_STR(resource->url().string()) : "<not found>");

    m_session->registerNetworkDataTask(*this);

    if (resource)
        m_eventStream = resource->eventStream();
}

NetworkDataTaskReplay::~NetworkDataTaskReplay()
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    m_session->unregisterNetworkDataTask(*this);
}

void NetworkDataTaskReplay::resume()
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Running;

    if (m_scheduledFailureType != NoFailure) {
        ASSERT(m_failureTimer.isActive());
        return;
    }

    enqueueEventHandler();
}

void NetworkDataTaskReplay::suspend()
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Suspended;
}

void NetworkDataTaskReplay::cancel()
{
    DEBUG_LOG("");

    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;
}

void NetworkDataTaskReplay::complete()
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    if (m_state == State::Completed)
        return;

    m_state = State::Completed;
    m_resource = nullptr;
}

void NetworkDataTaskReplay::invalidateAndCancel()
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    cancel();
    complete();
}

void NetworkDataTaskReplay::enqueueEventHandler()
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] {
        DEBUG_LOG("enqueueEventHandler callback");

        if (m_state == State::Suspended)
            return;

        if (m_state == State::Canceling || m_state == State::Completed || !m_client) {
            complete();
            return;
        }

        if (!m_resource) {
            DEBUG_LOG_ERROR("Error loading resource: could not find cached resource, URL = " STRING_SPECIFIER, DEBUG_STR(m_currentRequest.url().string()));
            didFinish(Error::NotFoundError); // TODO: Turn this into a 404?
            return;
        }

        if (!equalLettersIgnoringASCIICase(m_currentRequest.httpMethod(), "get")) {
            DEBUG_LOG_ERROR("Error loading resource: unsupported method (" STRING_SPECIFIER "), URL = " STRING_SPECIFIER, DEBUG_STR(m_currentRequest.httpMethod()), DEBUG_STR(m_currentRequest.url().string()));
            didFinish(Error::MethodNotAllowed);
            return;
        }

        auto event = m_eventStream.nextEvent();
        if (!event) {
            DEBUG_LOG_ERROR("Error loading resource: nextEvent return null, URL = " STRING_SPECIFIER, DEBUG_STR(m_currentRequest.url().string()));
            didFinish(Error::NotFoundError); // TODO: Turn this into a 404?
            return;
        }

        const auto visitor = WTF::makeVisitor(
            [this](const RequestSentEvent& event) {
                replayRequestSent(event);
            },
            [this](const ResponseReceivedEvent& event) {
                replayResponseReceived(event);
            },
            [this](const RedirectReceivedEvent& event) {
                replayRedirectReceived(event);
            },
            [this](const RedirectSentEvent& event) {
                replayRedirectSent(event);
            },
            [this](const DataReceivedEvent& event) {
                replayDataReceived(event);
            },
            [this](const FinishedEvent& event) {
                replayFinished(event);
            });

        WTF::visit(visitor, *event);
    });
}

void NetworkDataTaskReplay::replayRequestSent(const RequestSentEvent& event)
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    enqueueEventHandler();
}

void NetworkDataTaskReplay::replayResponseReceived(const ResponseReceivedEvent& event)
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    WebCore::ResourceResponse response(event.response);
    didReceiveResponse(WTFMove(response));
}

void NetworkDataTaskReplay::replayRedirectReceived(const RedirectReceivedEvent& event)
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    WebCore::ResourceResponse receivedResponse = event.response;
    WebCore::ResourceRequest receivedRequest = event.request;

    ASSERT(m_client);
    m_client->willPerformHTTPRedirection(WTFMove(receivedResponse), WTFMove(receivedRequest), [this, protectedThis = makeRef(*this)] (const WebCore::ResourceRequest& updatedRequest) {
        DEBUG_LOG("replayRedirectReceived callback: URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

        m_currentRequest = updatedRequest;
        enqueueEventHandler();
    });
}

void NetworkDataTaskReplay::replayRedirectSent(const RedirectSentEvent& event)
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    enqueueEventHandler();
}

void NetworkDataTaskReplay::replayDataReceived(const DataReceivedEvent& event)
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    ASSERT(m_client);
    m_client->didReceiveData(event.data.copyRef());

    enqueueEventHandler();
}

void NetworkDataTaskReplay::replayFinished(const FinishedEvent& event)
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    didFinish(event.error);
}

void NetworkDataTaskReplay::didReceiveResponse(WebCore::ResourceResponse&& response)
{
    DEBUG_LOG("URL = " STRING_SPECIFIER, DEBUG_STR(m_firstRequest.url().string()));

    ASSERT(m_client);
    m_client->didReceiveResponseNetworkSession(WTFMove(response), [this, protectedThis = makeRef(*this)](WebCore::PolicyAction policyAction) {
        DEBUG_LOG("didReceiveResponse callback (%u)", static_cast<unsigned>(policyAction));

        if (m_state == State::Canceling || m_state == State::Completed) {
            complete();
            return;
        }

        switch (policyAction) {
        case WebCore::PolicyAction::Use:
            enqueueEventHandler();
            break;
        case WebCore::PolicyAction::Suspend:
            LOG_ERROR("PolicyAction::Suspend encountered - Treating as PolicyAction::Ignore for now");
            FALLTHROUGH;
        case WebCore::PolicyAction::Ignore:
            complete();
            break;
        case WebCore::PolicyAction::Download:
            DEBUG_LOG_ERROR("WebCore::PolicyAction::PolicyDownload");
            break;
        }
    });
}

void NetworkDataTaskReplay::didFinish()
{
    didFinish({ });
}

void NetworkDataTaskReplay::didFinish(Error errorCode)
{
    didFinish(WebCore::ResourceError(webKitRelayDomain, static_cast<int>(errorCode), m_firstRequest.url(), String()));
}

void NetworkDataTaskReplay::didFinish(const WebCore::ResourceError& error)
{
    DEBUG_LOG("(%d)", error.errorCode());

    complete();

    ASSERT(m_client);
    m_client->didCompleteWithError(error);
}

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
