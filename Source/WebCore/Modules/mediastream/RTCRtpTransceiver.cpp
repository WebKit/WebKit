/*
 * Copyright (C) 2016 Ericsson AB. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCRtpTransceiver.h"

#if ENABLE(WEB_RTC)

#include "ContextDestructionObserverInlines.h"
#include "Logging.h"
#include "RTCPeerConnection.h"
#include "ScriptWrappableInlines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RTCRtpTransceiver);

RTCRtpTransceiver::RTCRtpTransceiver(Ref<RTCRtpSender>&& sender, Ref<RTCRtpReceiver>&& receiver, UniqueRef<RTCRtpTransceiverBackend>&& backend)
    : m_sender(WTF::move(sender))
    , m_receiver(WTF::move(receiver))
    , m_backend(WTF::move(backend))
{
}

RTCRtpTransceiver::~RTCRtpTransceiver() = default;

String RTCRtpTransceiver::mid() const
{
    return m_backend->mid();
}

RTCRtpTransceiverDirection RTCRtpTransceiver::direction() const
{
    return m_backend->direction();
}

std::optional<RTCRtpTransceiverDirection> RTCRtpTransceiver::currentDirection() const
{
    return m_backend->currentDirection();
}

ExceptionOr<void> RTCRtpTransceiver::setDirection(RTCRtpTransceiverDirection direction)
{
    if (m_isStopping)
        return Exception { ExceptionCode::InvalidStateError, "RTCRtpTransceiver has been stopped"_s };

    if (direction == RTCRtpTransceiverDirection::Stopped && this->direction() != RTCRtpTransceiverDirection::Stopped)
        return Exception { ExceptionCode::TypeError, "RTCRtpTransceiver direction is stopped"_s };

    m_backend->setDirection(direction);
    return { };
}

void RTCRtpTransceiver::setConnection(RTCPeerConnection& connection)
{
    ASSERT(!m_connection);
    m_connection = connection;
}

ExceptionOr<void> RTCRtpTransceiver::stop()
{
    if (!m_connection || m_connection->isClosed())
        return Exception { ExceptionCode::InvalidStateError, "RTCPeerConnection is closed"_s };

    if (m_isStopping)
        return { };

    m_isStopping = true;
    m_receiver->stop();
    m_sender->stop();
    m_backend->stop();

    // No need to call negotiation needed, it will be done by the backend itself.
    return { };
}

ExceptionOr<void> RTCRtpTransceiver::setCodecPreferences(const Vector<RTCRtpCodecCapability>& codecs)
{
    RELEASE_LOG_INFO(WebRTC, "RTCRtpTransceiver::setCodecPreferences");
    return m_backend->setCodecPreferences(codecs);
}

bool RTCRtpTransceiver::stopped() const
{
    return m_backend->stopped();
}

void RtpTransceiverSet::append(Ref<RTCRtpTransceiver>&& transceiver)
{
    m_transceivers.append(WTF::move(transceiver));
}

void RtpTransceiverSet::remove(const RTCRtpTransceiver& transceiver)
{
    m_transceivers.removeFirstMatching([&](auto& existing) {
        return existing.ptr() == &transceiver;
    });
}

Vector<std::reference_wrapper<RTCRtpSender>> RtpTransceiverSet::senders() const
{
    Vector<std::reference_wrapper<RTCRtpSender>> senders;
    for (auto& transceiver : m_transceivers) {
        if (transceiver->stopped())
            continue;
        senders.append(transceiver->sender());
    }
    return senders;
}

Vector<std::reference_wrapper<RTCRtpReceiver>> RtpTransceiverSet::receivers() const
{
    Vector<std::reference_wrapper<RTCRtpReceiver>> receivers;
    for (auto& transceiver : m_transceivers) {
        if (transceiver->stopped())
            continue;
        receivers.append(transceiver->receiver());
    }
    return receivers;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
