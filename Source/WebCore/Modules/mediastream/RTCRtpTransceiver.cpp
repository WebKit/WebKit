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

const String& RTCRtpTransceiver::mid() const
{
    if (!m_backend)
        return m_mid;
    return m_backend->mid();
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

void RtpTransceiverSet::append(Ref<RTCRtpTransceiver>&& transceiver)
{
    m_senders.append(transceiver->sender());
    m_receivers.append(transceiver->receiver());

    m_transceivers.append(WTFMove(transceiver));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
