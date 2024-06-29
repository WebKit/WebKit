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

#pragma once

#if USE(LIBWEBRTC)

#include <WebCore/LibWebRTCMacros.h>
#include <wtf/Forward.h>

ALLOW_COMMA_BEGIN
ALLOW_DEPRECATED_DECLARATIONS_BEGIN

#include <webrtc/rtc_base/async_packet_socket.h>

ALLOW_DEPRECATED_DECLARATIONS_END
ALLOW_COMMA_END

namespace WebKit {

struct RTCPacketOptions {
    enum class DifferentiatedServicesCodePoint : int8_t {
        NoChange = -1,
        Default = 0, // Same as CS0.
        CS0 = 0, // The default.
        CS1 = 8, // Bulk/background traffic.
        AF11 = 10,
        AF12 = 12,
        AF13 = 14,
        CS2 = 16,
        AF21 = 18,
        AF22 = 20,
        AF23 = 22,
        CS3 = 24,
        AF31 = 26,
        AF32 = 28,
        AF33 = 30,
        CS4 = 32,
        AF41 = 34, // Video.
        AF42 = 36, // Video.
        AF43 = 38, // Video.
        CS5 = 40, // Video.
        EF = 46, // Voice.
        CS6 = 48, // Voice.
        CS7 = 56 // Control messages.
    };

    struct SerializableData {
        DifferentiatedServicesCodePoint dscp;
        int32_t packetId;
        int rtpSendtimeExtensionId;
        int64_t srtpAuthTagLength;
        std::span<const char> srtpAuthKey;
        int64_t srtpPacketIndex;
    };

    explicit RTCPacketOptions(const rtc::PacketOptions& options)
        : options(options)
    { }

    explicit RTCPacketOptions(const SerializableData&);

    SerializableData serializableData() const;

    rtc::PacketOptions options;
};

}

#endif // USE(LIBWEBRTC)
