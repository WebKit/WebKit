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

#include "DataReference.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/LibWebRTCEnumTraits.h>
#include <wtf/Optional.h>

namespace WebKit {

void RTCPacketOptions::encode(IPC::Encoder& encoder) const
{
    encoder.encodeEnum(options.dscp);
    encoder << safeCast<int32_t>(options.packet_id);
    encoder << options.packet_time_params.rtp_sendtime_extension_id;

    encoder << static_cast<int64_t>(options.packet_time_params.srtp_auth_tag_len);
    if (options.packet_time_params.srtp_auth_tag_len > 0)
        encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(options.packet_time_params.srtp_auth_key.data()), options.packet_time_params.srtp_auth_key.size());

    encoder << options.packet_time_params.srtp_packet_index;
}

Optional<RTCPacketOptions> RTCPacketOptions::decode(IPC::Decoder& decoder)
{
    rtc::PacketTimeUpdateParams params;
    rtc::PacketOptions options;

    rtc::DiffServCodePoint dscp;
    if (!decoder.decodeEnum(dscp))
        return WTF::nullopt;
    options.dscp = dscp;

    Optional<int32_t> packetId;
    decoder >> packetId;
    if (!packetId)
        return WTF::nullopt;
    options.packet_id = packetId.value();

    Optional<int> rtpSendtimeExtensionId;
    decoder >> rtpSendtimeExtensionId;
    if (!rtpSendtimeExtensionId)
        return WTF::nullopt;
    params.rtp_sendtime_extension_id = rtpSendtimeExtensionId.value();

    Optional<int64_t> srtpAuthTagLength;
    decoder >> srtpAuthTagLength;
    if (!srtpAuthTagLength)
        return WTF::nullopt;
    params.srtp_auth_tag_len = srtpAuthTagLength.value();

    if (params.srtp_auth_tag_len > 0) {
        IPC::DataReference srtpAuthKey;
        if (!decoder.decode(srtpAuthKey))
            return WTF::nullopt;

        params.srtp_auth_key = std::vector<char>(static_cast<size_t>(srtpAuthKey.size()));
        memcpy(params.srtp_auth_key.data(), reinterpret_cast<const char*>(srtpAuthKey.data()), srtpAuthKey.size() * sizeof(char));
    }

    Optional<int64_t> srtpPacketIndex;
    decoder >> srtpPacketIndex;
    if (!srtpPacketIndex)
        return WTF::nullopt;
    params.srtp_packet_index = srtpPacketIndex.value();

    options.packet_time_params = WTFMove(params);
    return RTCPacketOptions { WTFMove(options) };
}

}

#endif // USE(LIBWEBRTC)
