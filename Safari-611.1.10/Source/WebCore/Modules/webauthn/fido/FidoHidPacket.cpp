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
#include "FidoHidPacket.h"

#if ENABLE(WEB_AUTHN)

#include <algorithm>

namespace fido {

FidoHidPacket::FidoHidPacket(Vector<uint8_t>&& data, uint32_t channelId)
    : m_data(WTFMove(data))
    , m_channelId(channelId)
{
}

// static
std::unique_ptr<FidoHidInitPacket> FidoHidInitPacket::createFromSerializedData(const Vector<uint8_t>& serialized, size_t* remainingSize)
{
    if (!remainingSize || serialized.size() != kHidPacketSize)
        return nullptr;

    size_t index = 0;
    auto channelId = (serialized[index++] & 0xff) << 24;
    channelId |= (serialized[index++] & 0xff) << 16;
    channelId |= (serialized[index++] & 0xff) << 8;
    channelId |= serialized[index++] & 0xff;

    auto command = static_cast<FidoHidDeviceCommand>(serialized[index++] & 0x7f);
    if (!isFidoHidDeviceCommand(command))
        return nullptr;

    uint16_t payloadSize = serialized[index++] << 8;
    payloadSize |= serialized[index++];

    // Check to see if payload is less than maximum size and padded with 0s.
    uint16_t dataSize = std::min(payloadSize, static_cast<uint16_t>(kHidPacketSize - index));

    // Update remaining size to determine the payload size of follow on packets.
    *remainingSize = payloadSize - dataSize;

    auto data = Vector<uint8_t>();
    data.append(serialized.begin() + index, dataSize);

    return makeUnique<FidoHidInitPacket>(channelId, command, WTFMove(data), payloadSize);
}

// U2F Initialization packet is defined as:
// Offset Length
// 0      4       Channel ID
// 4      1       Command ID
// 5      1       High order packet payload size
// 6      1       Low order packet payload size
// 7      (s-7)   Payload data
FidoHidInitPacket::FidoHidInitPacket(uint32_t channelId, FidoHidDeviceCommand cmd, Vector<uint8_t>&& data, uint16_t payloadLength)
    : FidoHidPacket(WTFMove(data), channelId)
    , m_command(cmd)
    , m_payloadLength(payloadLength)
{
}

Vector<uint8_t> FidoHidInitPacket::getSerializedData() const
{
    Vector<uint8_t> serialized;
    serialized.reserveInitialCapacity(kHidPacketSize);
    serialized.append((m_channelId >> 24) & 0xff);
    serialized.append((m_channelId >> 16) & 0xff);
    serialized.append((m_channelId >> 8) & 0xff);
    serialized.append(m_channelId & 0xff);
    serialized.append(static_cast<uint8_t>(m_command) | 0x80);
    serialized.append((m_payloadLength >> 8) & 0xff);
    serialized.append(m_payloadLength & 0xff);
    serialized.append(m_data.begin(), m_data.size());
    auto offset = serialized.size();
    serialized.grow(kHidPacketSize);
    memset(serialized.data() + offset, 0, kHidPacketSize - offset);

    return serialized;
}

// static
std::unique_ptr<FidoHidContinuationPacket> FidoHidContinuationPacket::createFromSerializedData(const Vector<uint8_t>& serialized, size_t* remainingSize)
{
    if (!remainingSize || serialized.size() != kHidPacketSize)
        return nullptr;

    size_t index = 0;
    auto channelId = (serialized[index++] & 0xff) << 24;
    channelId |= (serialized[index++] & 0xff) << 16;
    channelId |= (serialized[index++] & 0xff) << 8;
    channelId |= serialized[index++] & 0xff;
    auto sequence = serialized[index++];

    // Check to see if packet payload is less than maximum size and padded with 0s.
    size_t dataSize = std::min(*remainingSize, kHidPacketSize - index);
    *remainingSize -= dataSize;
    auto data = Vector<uint8_t>();
    data.append(serialized.begin() + index, dataSize);

    return makeUnique<FidoHidContinuationPacket>(channelId, sequence, WTFMove(data));
}

// U2F Continuation packet is defined as:
// Offset Length
// 0      4       Channel ID
// 4      1       Packet sequence 0x00..0x7f
// 5      (s-5)   Payload data
FidoHidContinuationPacket::FidoHidContinuationPacket(const uint32_t channelId, const uint8_t sequence, Vector<uint8_t>&& data)
    : FidoHidPacket(WTFMove(data), channelId)
    , m_sequence(sequence)
{
}

Vector<uint8_t> FidoHidContinuationPacket::getSerializedData() const
{
    Vector<uint8_t> serialized;
    serialized.reserveInitialCapacity(kHidPacketSize);
    serialized.append((m_channelId >> 24) & 0xff);
    serialized.append((m_channelId >> 16) & 0xff);
    serialized.append((m_channelId >> 8) & 0xff);
    serialized.append(m_channelId & 0xff);
    serialized.append(m_sequence);
    serialized.append(m_data.begin(), m_data.size());
    auto offset = serialized.size();
    serialized.grow(kHidPacketSize);
    memset(serialized.data() + offset, 0, kHidPacketSize - offset);

    return serialized;
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
