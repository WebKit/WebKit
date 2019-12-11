// Copyright 2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "ApduCommand.h"

#include <wtf/Optional.h>

#if ENABLE(WEB_AUTHN)

namespace apdu {

namespace {

// APDU command data length is 2 bytes encoded in big endian order.
uint16_t parseMessageLength(const Vector<uint8_t>& message, size_t offset)
{
    ASSERT(message.size() >= offset + 2);
    return (message[offset] << 8) | message[offset + 1];
}

} // namespace

Optional<ApduCommand> ApduCommand::createFromMessage(const Vector<uint8_t>& message)
{
    if (message.size() < kApduMinHeader || message.size() > kApduMaxLength)
        return WTF::nullopt;

    uint8_t cla = message[0];
    uint8_t ins = message[1];
    uint8_t p1 = message[2];
    uint8_t p2 = message[3];

    size_t responseLength = 0;
    Vector<uint8_t> data;

    switch (message.size()) {
    // No data present; no expected response.
    case kApduMinHeader:
        break;
    // Invalid encoding sizes.
    case kApduMinHeader + 1:
    case kApduMinHeader + 2:
        return WTF::nullopt;
    // No data present; response expected.
    case kApduMinHeader + 3:
        // Fifth byte must be 0.
        if (message[4])
            return WTF::nullopt;
        responseLength = parseMessageLength(message, kApduCommandLengthOffset);
        // Special case where response length of 0x0000 corresponds to 65536
        // as defined in ISO7816-4.
        if (!responseLength)
            responseLength = kApduMaxResponseLength;
        break;
    default:
        // Fifth byte must be 0.
        if (message[4])
            return WTF::nullopt;
        auto dataLength = parseMessageLength(message, kApduCommandLengthOffset);

        if (message.size() == dataLength + kApduCommandDataOffset) {
            // No response expected.
            data.appendRange(message.begin() + kApduCommandDataOffset, message.end());
        } else if (message.size() == dataLength + kApduCommandDataOffset + 2) {
            // Maximum response size is stored in final 2 bytes.
            data.appendRange(message.begin() + kApduCommandDataOffset, message.end() - 2);
            auto responseLengthOffset = kApduCommandDataOffset + dataLength;
            responseLength = parseMessageLength(message, responseLengthOffset);
            // Special case where response length of 0x0000 corresponds to 65536
            // as defined in ISO7816-4.
            if (!responseLength)
                responseLength = kApduMaxResponseLength;
        } else
            return WTF::nullopt;
        break;
    }

    return ApduCommand(cla, ins, p1, p2, responseLength, WTFMove(data));
}

ApduCommand::ApduCommand(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, size_t responseLength, Vector<uint8_t>&& data)
    : m_cla(cla)
    , m_ins(ins)
    , m_p1(p1)
    , m_p2(p2)
    , m_responseLength(responseLength)
    , m_data(WTFMove(data))
{
}

Vector<uint8_t> ApduCommand::getEncodedCommand() const
{
    Vector<uint8_t> encoded = { m_cla, m_ins, m_p1, m_p2 };

    // If data exists, request size (Lc) is encoded in 3 bytes, with the first
    // byte always being null, and the other two bytes being a big-endian
    // representation of the request size. If data length is 0, response size (Le)
    // will be prepended with a null byte.
    if (!m_data.isEmpty()) {
        size_t dataLength = m_data.size();

        encoded.append(0x0);
        if (dataLength > kApduMaxDataLength)
            dataLength = kApduMaxDataLength;
        encoded.append((dataLength >> 8) & 0xff);
        encoded.append(dataLength & 0xff);
        encoded.appendRange(m_data.begin(), m_data.begin() + dataLength);
    } else if (m_responseLength > 0)
        encoded.append(0x0);

    if (m_responseLength > 0) {
        size_t responseLength = m_responseLength;
        if (responseLength > kApduMaxResponseLength)
            responseLength = kApduMaxResponseLength;
        // A zero value represents a response length of 65,536 bytes.
        encoded.append((responseLength >> 8) & 0xff);
        encoded.append(responseLength & 0xff);
    }
    return encoded;
}

} // namespace apdu

#endif // ENABLE(WEB_AUTHN)
