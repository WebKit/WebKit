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

#include <wtf/NeverDestroyed.h>

namespace WebCore {

RTCRtpTransceiver::RTCRtpTransceiver(Ref<RTCRtpSender>&& sender, Ref<RTCRtpReceiver>&& receiver, std::unique_ptr<RTCRtpTransceiverBackend>&& backend)
    : m_direction(RTCRtpTransceiverDirection::Sendrecv)
    , m_sender(WTFMove(sender))
    , m_receiver(WTFMove(receiver))
    , m_iceTransport(RTCIceTransport::create())
    , m_backend(WTFMove(backend))
{
}

String RTCRtpTransceiver::mid() const
{
    return m_backend ? m_backend->mid() : String { };
}

bool RTCRtpTransceiver::hasSendingDirection() const
{
    return m_direction == RTCRtpTransceiverDirection::Sendrecv || m_direction == RTCRtpTransceiverDirection::Sendonly;
}

RTCRtpTransceiverDirection RTCRtpTransceiver::direction() const
{
    if (!m_backend)
        return m_direction;
    return m_backend->direction();
}

std::optional<RTCRtpTransceiverDirection> RTCRtpTransceiver::currentDirection() const
{
    if (!m_backend)
        return std::nullopt;
    return m_backend->currentDirection();
}

void RTCRtpTransceiver::setDirection(RTCRtpTransceiverDirection direction)
{
    m_direction = direction;
    if (m_backend)
        m_backend->setDirection(direction);
}


void RTCRtpTransceiver::enableSendingDirection()
{
    if (m_direction == RTCRtpTransceiverDirection::Recvonly)
        m_direction = RTCRtpTransceiverDirection::Sendrecv;
    else if (m_direction == RTCRtpTransceiverDirection::Inactive)
        m_direction = RTCRtpTransceiverDirection::Sendonly;
}

void RTCRtpTransceiver::disableSendingDirection()
{
    if (m_direction == RTCRtpTransceiverDirection::Sendrecv)
        m_direction = RTCRtpTransceiverDirection::Recvonly;
    else if (m_direction == RTCRtpTransceiverDirection::Sendonly)
        m_direction = RTCRtpTransceiverDirection::Inactive;
}

void RTCRtpTransceiver::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_receiver->stop();
    m_sender->stop();
    if (m_backend)
        m_backend->stop();
}

bool RTCRtpTransceiver::stopped() const
{
    if (m_backend)
        return m_backend->stopped();
    return m_stopped;
}

void RtpTransceiverSet::append(Ref<RTCRtpTransceiver>&& transceiver)
{
    m_transceivers.append(WTFMove(transceiver));
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
