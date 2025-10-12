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
#include "RTCPacketOptions.h"

#if USE(LIBWEBRTC)


namespace WebKit {

static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::NoChange) == webrtc::DSCP_NO_CHANGE);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::Default) == webrtc::DSCP_DEFAULT);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS0) == webrtc::DSCP_CS0);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS1) == webrtc::DSCP_CS1);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF11) == webrtc::DSCP_AF11);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF12) == webrtc::DSCP_AF12);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF13) == webrtc::DSCP_AF13);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS2) == webrtc::DSCP_CS2);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF21) == webrtc::DSCP_AF21);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF22) == webrtc::DSCP_AF22);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF23) == webrtc::DSCP_AF23);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS3) == webrtc::DSCP_CS3);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF31) == webrtc::DSCP_AF31);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF32) == webrtc::DSCP_AF32);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF33) == webrtc::DSCP_AF33);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS4) == webrtc::DSCP_CS4);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF41) == webrtc::DSCP_AF41);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF42) == webrtc::DSCP_AF42);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::AF43) == webrtc::DSCP_AF43);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS5) == webrtc::DSCP_CS5);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::EF) == webrtc::DSCP_EF);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS6) == webrtc::DSCP_CS6);
static_assert(static_cast<webrtc::DiffServCodePoint>(RTCPacketOptions::DifferentiatedServicesCodePoint::CS7) == webrtc::DSCP_CS7);

static RTCPacketOptions::DifferentiatedServicesCodePoint toDifferentiatedServicesCodePoint(webrtc::DiffServCodePoint dscp)
{
    switch (dscp) {
    case webrtc::DSCP_NO_CHANGE:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::NoChange;
    case webrtc::DSCP_CS0:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS0;
    case webrtc::DSCP_CS1:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS1;
    case webrtc::DSCP_AF11:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF11;
    case webrtc::DSCP_AF12:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF12;
    case webrtc::DSCP_AF13:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF13;
    case webrtc::DSCP_CS2:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS2;
    case webrtc::DSCP_AF21:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF21;
    case webrtc::DSCP_AF22:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF22;
    case webrtc::DSCP_AF23:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF23;
    case webrtc::DSCP_CS3:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS3;
    case webrtc::DSCP_AF31:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF31;
    case webrtc::DSCP_AF32:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF32;
    case webrtc::DSCP_AF33:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF33;
    case webrtc::DSCP_CS4:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS4;
    case webrtc::DSCP_AF41:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF41;
    case webrtc::DSCP_AF42:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF42;
    case webrtc::DSCP_AF43:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::AF43;
    case webrtc::DSCP_CS5:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS5;
    case webrtc::DSCP_EF:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::EF;
    case webrtc::DSCP_CS6:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS6;
    case webrtc::DSCP_CS7:
        return RTCPacketOptions::DifferentiatedServicesCodePoint::CS7;
    }
    ASSERT_NOT_REACHED();
    return RTCPacketOptions::DifferentiatedServicesCodePoint::Default;
}

RTCPacketOptions::RTCPacketOptions(const SerializableData& data)
{
    options.dscp = static_cast<webrtc::DiffServCodePoint>(data.dscp);
    options.packet_id = data.packetId;

    webrtc::PacketTimeUpdateParams params;
    params.rtp_sendtime_extension_id = data.rtpSendtimeExtensionId;
    params.srtp_auth_tag_len = data.srtpAuthTagLength;
    if (data.srtpAuthTagLength > 0)
        params.srtp_auth_key = std::vector<char>(data.srtpAuthKey.begin(), data.srtpAuthKey.end());
    params.srtp_packet_index = data.srtpPacketIndex;

    options.packet_time_params = WTFMove(params);
}

auto RTCPacketOptions::serializableData() const -> SerializableData
{
    return {
        toDifferentiatedServicesCodePoint(options.dscp),
        safeCast<int32_t>(options.packet_id),
        options.packet_time_params.rtp_sendtime_extension_id,
        static_cast<int64_t>(options.packet_time_params.srtp_auth_tag_len),
        options.packet_time_params.srtp_auth_tag_len > 0  ? std::span<const char> { } : std::span<const char> { options.packet_time_params.srtp_auth_key },
        options.packet_time_params.srtp_packet_index
    };
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
