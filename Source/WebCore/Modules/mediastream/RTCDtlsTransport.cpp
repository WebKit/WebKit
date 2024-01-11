/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "RTCDtlsTransport.h"

#if ENABLE(WEB_RTC)

#include "Blob.h"
#include "ContextDestructionObserverInlines.h"
#include "EventNames.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "RTCDtlsTransportBackend.h"
#include "RTCIceTransport.h"
#include "RTCPeerConnection.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCDtlsTransport);

Ref<RTCDtlsTransport> RTCDtlsTransport::create(ScriptExecutionContext& context, UniqueRef<RTCDtlsTransportBackend>&& backend, Ref<RTCIceTransport>&& iceTransport)
{
    auto result = adoptRef(*new RTCDtlsTransport(context, WTFMove(backend), WTFMove(iceTransport)));
    result->suspendIfNeeded();
    return result;
}

RTCDtlsTransport::RTCDtlsTransport(ScriptExecutionContext& context, UniqueRef<RTCDtlsTransportBackend>&& backend, Ref<RTCIceTransport>&& iceTransport)
    : ActiveDOMObject(&context)
    , m_backend(WTFMove(backend))
    , m_iceTransport(WTFMove(iceTransport))
{
    m_backend->registerClient(*this);
}

RTCDtlsTransport::~RTCDtlsTransport()
{
    m_backend->unregisterClient();
}

Vector<Ref<JSC::ArrayBuffer>> RTCDtlsTransport::getRemoteCertificates()
{
    return m_remoteCertificates;
}

void RTCDtlsTransport::stop()
{
    m_state = RTCDtlsTransportState::Closed;
}

bool RTCDtlsTransport::virtualHasPendingActivity() const
{
    return m_state != RTCDtlsTransportState::Closed && hasEventListeners();
}

void RTCDtlsTransport::onStateChanged(RTCDtlsTransportState state, Vector<Ref<JSC::ArrayBuffer>>&& certificates)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, state, certificates = WTFMove(certificates)]() mutable {
        if (m_state == RTCDtlsTransportState::Closed)
            return;

        if (m_remoteCertificates != certificates)
            m_remoteCertificates = WTFMove(certificates);

        if (m_state != state) {
            m_state = state;
            if (RefPtr connection = m_iceTransport->connection())
                connection->updateConnectionState();
            dispatchEvent(Event::create(eventNames().statechangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
        }
    });
}

void RTCDtlsTransport::onError()
{
    onStateChanged(RTCDtlsTransportState::Failed, { });
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
