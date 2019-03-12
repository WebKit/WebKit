/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <webrtc/rtc_base/network.h>
#include <wtf/Forward.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct RTCNetwork {
    RTCNetwork() = default;
    explicit RTCNetwork(const rtc::Network&);

    rtc::Network value() const;

    void encode(IPC::Encoder&) const;
    static Optional<RTCNetwork> decode(IPC::Decoder&);

    struct IPAddress {
        IPAddress() = default;
        explicit IPAddress(const rtc::IPAddress& address): value(address) { }

        void encode(IPC::Encoder&) const;
        static Optional<IPAddress> decode(IPC::Decoder&);

        rtc::IPAddress value;
    };

    static rtc::SocketAddress isolatedCopy(const rtc::SocketAddress&);

    struct SocketAddress {
        SocketAddress() = default;
        explicit SocketAddress(const rtc::SocketAddress& address): value(address) { }

        void encode(IPC::Encoder&) const;
        static Optional<SocketAddress> decode(IPC::Decoder&);

        rtc::SocketAddress value;
    };

    std::string name;
    std::string description;
    IPAddress prefix;
    int prefixLength;
    int type;
    uint16_t id;
    int preference;
    bool active;
    bool ignored;
    int scopeID;
    std::string key;
    size_t length;
    std::vector<rtc::InterfaceAddress> ips;
};

}

#endif // USE(LIBWEBRTC)
