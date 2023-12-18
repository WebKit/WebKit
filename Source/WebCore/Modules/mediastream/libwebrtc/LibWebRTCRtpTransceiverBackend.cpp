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

#include "JSDOMPromiseDeferred.h"
#include "LibWebRTCRtpReceiverBackend.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "LibWebRTCUtils.h"
#include "RTCRtpCodecCapability.h"

namespace WebCore {

std::unique_ptr<LibWebRTCRtpReceiverBackend> LibWebRTCRtpTransceiverBackend::createReceiverBackend()
{
    return makeUnique<LibWebRTCRtpReceiverBackend>(m_rtcTransceiver->receiver());
}

std::unique_ptr<LibWebRTCRtpSenderBackend> LibWebRTCRtpTransceiverBackend::createSenderBackend(LibWebRTCPeerConnectionBackend& backend, LibWebRTCRtpSenderBackend::Source&& source)
{
    return makeUnique<LibWebRTCRtpSenderBackend>(backend, m_rtcTransceiver->sender(), WTFMove(source));
}

RTCRtpTransceiverDirection LibWebRTCRtpTransceiverBackend::direction() const
{
    return toRTCRtpTransceiverDirection(m_rtcTransceiver->direction());
}

std::optional<RTCRtpTransceiverDirection> LibWebRTCRtpTransceiverBackend::currentDirection() const
{
    auto value = m_rtcTransceiver->current_direction();
    if (!value)
        return std::nullopt;
    return toRTCRtpTransceiverDirection(*value);
}

void LibWebRTCRtpTransceiverBackend::setDirection(RTCRtpTransceiverDirection direction)
{
    // FIXME: Handle error.
    m_rtcTransceiver->SetDirectionWithError(fromRTCRtpTransceiverDirection(direction));
}

String LibWebRTCRtpTransceiverBackend::mid()
{
    if (auto mid = m_rtcTransceiver->mid())
        return fromStdString(*mid);
    return String { };
}

void LibWebRTCRtpTransceiverBackend::stop()
{
    m_rtcTransceiver->StopStandard();
}

bool LibWebRTCRtpTransceiverBackend::stopped() const
{
    return m_rtcTransceiver->stopped();
}

static inline ExceptionOr<webrtc::RtpCodecCapability> toRtpCodecCapability(const RTCRtpCodecCapability& codec)
{
    webrtc::RtpCodecCapability rtcCodec;
    if (codec.mimeType.startsWith("video/"_s))
        rtcCodec.kind = cricket::MEDIA_TYPE_VIDEO;
    else if (codec.mimeType.startsWith("audio/"_s))
        rtcCodec.kind = cricket::MEDIA_TYPE_AUDIO;
    else
        return Exception { ExceptionCode::InvalidModificationError, "RTCRtpCodecCapability bad mimeType"_s };

    rtcCodec.name = StringView(codec.mimeType).substring(6).utf8().toStdString();
    rtcCodec.clock_rate = codec.clockRate;
    if (codec.channels)
        rtcCodec.num_channels = *codec.channels;

    for (auto parameter : StringView(codec.sdpFmtpLine).split(';')) {
        auto position = parameter.find('=');
        if (position == notFound)
            return Exception { ExceptionCode::InvalidModificationError, "RTCRtpCodecCapability sdpFmtLine badly formated"_s };
        rtcCodec.parameters.emplace(parameter.left(position).utf8().data(), parameter.substring(position + 1).utf8().data());
    }

    return rtcCodec;
}

ExceptionOr<void> LibWebRTCRtpTransceiverBackend::setCodecPreferences(const Vector<RTCRtpCodecCapability>& codecs)
{
    std::vector<webrtc::RtpCodecCapability> rtcCodecs;
    for (auto& codec : codecs) {
        auto result = toRtpCodecCapability(codec);
        if (result.hasException())
            return result.releaseException();
        rtcCodecs.push_back(result.releaseReturnValue());
    }
    auto result = m_rtcTransceiver->SetCodecPreferences(rtcCodecs);
    if (!result.ok())
        return toException(result);
    return { };
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
