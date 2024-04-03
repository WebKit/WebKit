/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "RTCIceTransport.h"

#if ENABLE(WEB_RTC)

#include "ContextDestructionObserverInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "RTCPeerConnection.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCIceTransport);

Ref<RTCIceTransport> RTCIceTransport::create(ScriptExecutionContext& context, UniqueRef<RTCIceTransportBackend>&& backend, RTCPeerConnection& connection)
{
    auto result = adoptRef(*new RTCIceTransport(context, WTFMove(backend), connection));
    result->suspendIfNeeded();
    return result;
}

RTCIceTransport::RTCIceTransport(ScriptExecutionContext& context, UniqueRef<RTCIceTransportBackend>&& backend, RTCPeerConnection& connection)
    : ActiveDOMObject(&context)
    , m_backend(WTFMove(backend))
    , m_connection(connection)
{
    m_backend->registerClient(*this);
}

RTCIceTransport::~RTCIceTransport()
{
    m_backend->unregisterClient();
}

void RTCIceTransport::stop()
{
    m_isStopped = true;
    m_transportState = RTCIceTransportState::Closed;
}

bool RTCIceTransport::virtualHasPendingActivity() const
{
    return m_transportState != RTCIceTransportState::Closed && hasEventListeners();
}

void RTCIceTransport::onStateChanged(RTCIceTransportState state)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, state]() mutable {
        if (m_isStopped)
            return;

        if (m_transportState == state)
            return;

        m_transportState = state;
        if (auto connection = this->connection())
            connection->processIceTransportStateChange(*this);
    });
}

void RTCIceTransport::onGatheringStateChanged(RTCIceGatheringState state)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, state]() mutable {
        if (m_isStopped)
            return;

        if (m_gatheringState == state)
            return;

        m_gatheringState = state;
        dispatchEvent(Event::create(eventNames().gatheringstatechangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
    });
}

void RTCIceTransport::onSelectedCandidatePairChanged(RefPtr<RTCIceCandidate>&& local, RefPtr<RTCIceCandidate>&& remote)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, local = WTFMove(local), remote = WTFMove(remote)]() mutable {
        if (m_isStopped)
            return;

        m_selectedCandidatePair = CandidatePair { WTFMove(local), WTFMove(remote) };
        dispatchEvent(Event::create(eventNames().selectedcandidatepairchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
    });
}

std::optional<RTCIceTransport::CandidatePair> RTCIceTransport::getSelectedCandidatePair()
{
    if (m_transportState == RTCIceTransportState::Closed)
        return { };

    ASSERT(m_transportState != RTCIceTransportState::New || !m_selectedCandidatePair);
    return m_selectedCandidatePair;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
