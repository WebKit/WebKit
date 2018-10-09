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
#include "FidoHidMessage.h"

#if ENABLE(WEB_AUTHN)

#include "FidoParsingUtils.h"

namespace fido {

// static
std::optional<FidoHidMessage> FidoHidMessage::create(uint32_t channelId, FidoHidDeviceCommand type, const Vector<uint8_t>& data)
{
    if (data.size() > kHidMaxMessageSize)
        return std::nullopt;

    switch (type) {
    case FidoHidDeviceCommand::kPing:
        break;
    case FidoHidDeviceCommand::kMsg:
    case FidoHidDeviceCommand::kCbor: {
        if (data.isEmpty())
            return std::nullopt;
        break;
    }

    case FidoHidDeviceCommand::kCancel:
    case FidoHidDeviceCommand::kWink: {
        if (!data.isEmpty())
            return std::nullopt;
        break;
    }
    case FidoHidDeviceCommand::kLock: {
        if (data.size() != 1 || data[0] > kHidMaxLockSeconds)
            return std::nullopt;
        break;
    }
    case FidoHidDeviceCommand::kInit: {
        if (data.size() != 8)
            return std::nullopt;
        break;
    }
    case FidoHidDeviceCommand::kKeepAlive:
    case FidoHidDeviceCommand::kError:
        if (data.size() != 1)
            return std::nullopt;
    }

    return FidoHidMessage(channelId, type, data);
}

// static
std::optional<FidoHidMessage> FidoHidMessage::createFromSerializedData(const Vector<uint8_t>& serializedData)
{
    size_t remainingSize = 0;
    if (serializedData.size() > kHidPacketSize || serializedData.size() < kHidInitPacketHeaderSize)
        return std::nullopt;

    auto initPacket = FidoHidInitPacket::createFromSerializedData(serializedData, &remainingSize);

    if (!initPacket)
        return std::nullopt;

    return FidoHidMessage(WTFMove(initPacket), remainingSize);
}

bool FidoHidMessage::messageComplete() const
{
    return !m_remainingSize;
}

Vector<uint8_t> FidoHidMessage::getMessagePayload() const
{
    Vector<uint8_t> data;
    size_t dataSize = 0;
    for (const auto& packet : m_packets)
        dataSize += packet->getPacketPayload().size();
    data.reserveInitialCapacity(dataSize);

    for (const auto& packet : m_packets) {
        const auto& packet_data = packet->getPacketPayload();
        data.appendVector(packet_data);
    }

    return data;
}

Vector<uint8_t> FidoHidMessage::popNextPacket()
{
    if (m_packets.isEmpty())
        return { };

    Vector<uint8_t> data = m_packets.first()->getSerializedData();
    m_packets.removeFirst();
    return data;
}

bool FidoHidMessage::addContinuationPacket(const Vector<uint8_t>& buf)
{
    size_t remainingSize = m_remainingSize;
    auto contPacket = FidoHidContinuationPacket::createFromSerializedData(buf, &remainingSize);

    // Reject packets with a different channel id.
    if (!contPacket || m_channelId != contPacket->channelId())
        return false;

    m_remainingSize = remainingSize;
    m_packets.append(WTFMove(contPacket));
    return true;
}

size_t FidoHidMessage::numPackets() const
{
    return m_packets.size();
}

FidoHidMessage::FidoHidMessage(uint32_t channelId, FidoHidDeviceCommand type, const Vector<uint8_t>& data)
    : m_channelId(channelId)
{
    uint8_t sequence = 0;

    size_t pos = data.size() > kHidInitPacketDataSize ? kHidInitPacketDataSize : data.size();
    m_packets.append(std::make_unique<FidoHidInitPacket>(channelId, type, getInitPacketData(data), data.size()));
    for (; pos < data.size(); pos += kHidContinuationPacketDataSize)
        m_packets.append(std::make_unique<FidoHidContinuationPacket>(channelId, sequence++, getContinuationPacketData(data, pos)));
}

FidoHidMessage::FidoHidMessage(std::unique_ptr<FidoHidInitPacket> initPacket, size_t remainingSize)
    : m_remainingSize(remainingSize)
{
    m_channelId = initPacket->channelId();
    m_cmd = initPacket->command();
    m_packets.append(WTFMove(initPacket));
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
