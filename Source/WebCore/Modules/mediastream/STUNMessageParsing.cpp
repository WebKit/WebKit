/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "STUNMessageParsing.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include <LibWebRTCMacros.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/rtc_base/byte_order.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {
namespace WebRTC {

static inline bool isStunMessage(uint16_t messageType)
{
    // https://tools.ietf.org/html/rfc5389#section-6 for STUN messages.
    // TURN messages start by the channel number which is constrained by https://tools.ietf.org/html/rfc5766#section-11.
    return !(messageType & 0xC000);
}

std::optional<STUNMessageLengths> getSTUNOrTURNMessageLengths(std::span<const uint8_t> data)
{
    if (data.size() < 4)
        return { };

    auto messageType = be16toh(*reinterpret_cast<const uint16_t*>(data.data()));
    auto messageLength = be16toh(*reinterpret_cast<const uint16_t*>(data.data() + 2));

    // STUN data message header is 20 bytes.
    if (isStunMessage(messageType)) {
        size_t length = 20 + messageLength;
        return STUNMessageLengths { length, length };
    }

    // TURN data message header is 4 bytes plus padding bytes to get 4 bytes alignment as needed.
    size_t length = 4 + messageLength;
    size_t roundedLength = length % 4 ? (length + 4 - (length % 4)) : length;
    return STUNMessageLengths { length, roundedLength };
}

static inline Vector<uint8_t> extractSTUNOrTURNMessages(Vector<uint8_t>&& buffered, const Function<void(std::span<const uint8_t> data)>& processMessage)
{
    auto data = buffered.span();

    while (true) {
        auto lengths = getSTUNOrTURNMessageLengths(data);

        if (!lengths || lengths->messageLengthWithPadding > data.size()) {
            if (!data.size())
                return { };

            std::memcpy(buffered.data(), data.data(), data.size());
            buffered.resize(data.size());
            return WTFMove(buffered);
        }

        processMessage(data.first(lengths->messageLength));

        data = data.subspan(lengths->messageLengthWithPadding);
    }
}

static inline Vector<uint8_t> extractDataMessages(Vector<uint8_t>&& buffered, const Function<void(std::span<const uint8_t> data)>& processMessage)
{
    constexpr size_t lengthFieldSize = 2; // number of bytes read by be16toh.

    auto* data = buffered.data();
    size_t size = buffered.size();

    while (true) {
        bool canReadLength = size >= lengthFieldSize;
        size_t length = canReadLength ? be16toh(*reinterpret_cast<const uint16_t*>(data)) : 0;
        if (!canReadLength || length > size - lengthFieldSize) {
            if (!size)
                return { };

            std::memcpy(buffered.data(), data, size);
            buffered.resize(size);
            return WTFMove(buffered);
        }

        data += lengthFieldSize;
        size -= lengthFieldSize;

        processMessage(std::span { data, length });

        data += length;
        size -= length;
    }
}

Vector<uint8_t> extractMessages(Vector<uint8_t>&& buffer, MessageType type, const Function<void(std::span<const uint8_t> data)>& processMessage)
{
    return type == MessageType::STUN ? extractSTUNOrTURNMessages(WTFMove(buffer), processMessage) : extractDataMessages(WTFMove(buffer), processMessage);
}

} // namespace WebRTC
} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
