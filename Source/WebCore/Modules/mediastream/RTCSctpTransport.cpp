/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCSctpTransport.h"

#if ENABLE(WEB_RTC)

#include "ContextDestructionObserverInlines.h"
#include "EventNames.h"
#include "Logging.h"
#include "RTCDtlsTransport.h"
#include "RTCSctpTransportBackend.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCSctpTransport);

Ref<RTCSctpTransport> RTCSctpTransport::create(ScriptExecutionContext& context, UniqueRef<RTCSctpTransportBackend>&& backend, Ref<RTCDtlsTransport>&& transport)
{
    auto result = adoptRef(*new RTCSctpTransport(context, WTFMove(backend), WTFMove(transport)));
    result->suspendIfNeeded();
    return result;
}

RTCSctpTransport::RTCSctpTransport(ScriptExecutionContext& context, UniqueRef<RTCSctpTransportBackend>&& backend, Ref<RTCDtlsTransport >&& transport)
    : ActiveDOMObject(&context)
    , m_backend(WTFMove(backend))
    , m_transport(WTFMove(transport))
{
    m_backend->registerClient(*this);
}

RTCSctpTransport::~RTCSctpTransport()
{
    m_backend->unregisterClient();
}

void RTCSctpTransport::stop()
{
    m_state = RTCSctpTransportState::Closed;
}

bool RTCSctpTransport::virtualHasPendingActivity() const
{
    return m_state != RTCSctpTransportState::Closed && hasEventListeners();
}

void RTCSctpTransport::onStateChanged(RTCSctpTransportState state, std::optional<double> maxMessageSize, std::optional<unsigned short> maxChannels)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, state, maxMessageSize, maxChannels]() mutable {
        if (m_state == RTCSctpTransportState::Closed)
            return;

        if (maxMessageSize)
            m_maxMessageSize = *maxMessageSize;
        if (maxChannels)
            m_maxChannels = *maxChannels;

        if (m_state != state) {
            m_state = state;
            dispatchEvent(Event::create(eventNames().statechangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
        }
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
