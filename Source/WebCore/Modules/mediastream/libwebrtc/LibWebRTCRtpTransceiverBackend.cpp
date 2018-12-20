/*
 * Copyright (C) 2018 Apple Inc.
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
#include "LibWebRTCRtpTransceiverBackend.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCRtpReceiverBackend.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "LibWebRTCUtils.h"

namespace WebCore {

std::unique_ptr<LibWebRTCRtpReceiverBackend> LibWebRTCRtpTransceiverBackend::createReceiverBackend()
{
    return std::make_unique<LibWebRTCRtpReceiverBackend>(m_rtcTransceiver->receiver());
}

std::unique_ptr<LibWebRTCRtpSenderBackend> LibWebRTCRtpTransceiverBackend::createSenderBackend(LibWebRTCPeerConnectionBackend& backend, LibWebRTCRtpSenderBackend::Source&& source)
{
    return std::make_unique<LibWebRTCRtpSenderBackend>(backend, m_rtcTransceiver->sender(), WTFMove(source));
}

RTCRtpTransceiverDirection LibWebRTCRtpTransceiverBackend::direction() const
{
    return toRTCRtpTransceiverDirection(m_rtcTransceiver->direction());
}

Optional<RTCRtpTransceiverDirection> LibWebRTCRtpTransceiverBackend::currentDirection() const
{
    auto value = m_rtcTransceiver->current_direction();
    if (!value)
        return WTF::nullopt;
    return toRTCRtpTransceiverDirection(*value);
}

void LibWebRTCRtpTransceiverBackend::setDirection(RTCRtpTransceiverDirection direction)
{
    m_rtcTransceiver->SetDirection(fromRTCRtpTransceiverDirection(direction));
}

String LibWebRTCRtpTransceiverBackend::mid()
{
    if (auto mid = m_rtcTransceiver->mid())
        return fromStdString(*mid);
    return String { };
}

void LibWebRTCRtpTransceiverBackend::stop()
{
    m_rtcTransceiver->Stop();
}

bool LibWebRTCRtpTransceiverBackend::stopped() const
{
    return m_rtcTransceiver->stopped();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
