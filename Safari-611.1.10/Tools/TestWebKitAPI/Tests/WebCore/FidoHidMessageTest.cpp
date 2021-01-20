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

#if ENABLE(WEB_AUTHN)

#include <WebCore/FidoConstants.h>
#include <WebCore/FidoHidMessage.h>
#include <WebCore/FidoHidPacket.h>
#include <wtf/Deque.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace fido;

// Packets should be 64 bytes + 1 report ID byte.
TEST(FidoHidMessageTest, TestPacketSize)
{
    uint32_t channelId = 0x05060708;
    Vector<uint8_t> data;

    auto initPacket = makeUnique<FidoHidInitPacket>(channelId, FidoHidDeviceCommand::kInit, Vector<uint8_t>(data), data.size());
    EXPECT_EQ(64u, initPacket->getSerializedData().size());

    auto continuationPacket = makeUnique<FidoHidContinuationPacket>(channelId, 0, WTFMove(data));
    EXPECT_EQ(64u, continuationPacket->getSerializedData().size());
}

/*
 * CTAP HID Init Packets are of the format:
 * Byte 0-3:  Channel ID
 * Byte 4:    Command byte
 * Byte 5-6:  Big Endian size of data
 * Byte 7-n:  Data block
 *
 * Remaining buffer is padded with 0
 */
TEST(FidoHidMessageTest, TestPacketData1)
{
    uint32_t channelId = 0xF5060708;
    Vector<uint8_t> data {10, 11};
    FidoHidDeviceCommand cmd = FidoHidDeviceCommand::kWink;
    auto initPacket = makeUnique<FidoHidInitPacket>(channelId, cmd, Vector<uint8_t>(data), data.size());
    size_t index = 0;

    Vector<uint8_t> serialized = initPacket->getSerializedData();
    EXPECT_EQ((channelId >> 24) & 0xff, serialized[index++]);
    EXPECT_EQ((channelId >> 16) & 0xff, serialized[index++]);
    EXPECT_EQ((channelId >> 8) & 0xff, serialized[index++]);
    EXPECT_EQ(channelId & 0xff, serialized[index++]);
    EXPECT_EQ(static_cast<uint8_t>(cmd), serialized[index++] & 0x7f);

    EXPECT_EQ(data.size() >> 8, serialized[index++]);
    EXPECT_EQ(data.size() & 0xff, serialized[index++]);
    EXPECT_EQ(data[0], serialized[index++]);
    EXPECT_EQ(data[1], serialized[index++]);
    for (; index < serialized.size(); index++)
        EXPECT_EQ(0, serialized[index]) << "mismatch at index " << index;
}

/*
 * CTAP HID Continuation Packets are of the format:
 * Byte 0-3:  Channel ID
 * Byte 4:    SEQ
 * Byte 5-n:  Data block
 *
 * Remaining buffer is padded with 0
 */
TEST(FidoHidMessageTest, TestPacketData2)
{
    uint32_t channelId = 0xF5060708;
    Vector<uint8_t> data {10, 11};
    auto initPacket = makeUnique<FidoHidContinuationPacket>(channelId, 0, Vector<uint8_t>(data));
    size_t index = 0;

    Vector<uint8_t> serialized = initPacket->getSerializedData();
    EXPECT_EQ((channelId >> 24) & 0xff, serialized[index++]);
    EXPECT_EQ((channelId >> 16) & 0xff, serialized[index++]);
    EXPECT_EQ((channelId >> 8) & 0xff, serialized[index++]);
    EXPECT_EQ(channelId & 0xff, serialized[index++]);
    EXPECT_EQ(0, serialized[index++]);

    EXPECT_EQ(data[0], serialized[index++]);
    EXPECT_EQ(data[1], serialized[index++]);
    for (; index < serialized.size(); index++)
        EXPECT_EQ(0, serialized[index]) << "mismatch at index " << index;
}

TEST(FidoHidMessageTest, TestPacketConstructors)
{
    uint32_t channelId = 0x05060708;
    Vector<uint8_t> data {10, 11};
    FidoHidDeviceCommand cmd = FidoHidDeviceCommand::kWink;
    size_t length = data.size();
    auto origPacket = makeUnique<FidoHidInitPacket>(channelId, cmd, WTFMove(data), length);

    size_t payloadLength = static_cast<size_t>(origPacket->payloadLength());
    Vector<uint8_t> origData = origPacket->getSerializedData();

    auto reconstructedPacket = FidoHidInitPacket::createFromSerializedData(origData, &payloadLength);
    EXPECT_EQ(origPacket->command(), reconstructedPacket->command());
    EXPECT_EQ(origPacket->payloadLength(), reconstructedPacket->payloadLength());
    EXPECT_EQ(origPacket->getPacketPayload(), reconstructedPacket->getPacketPayload());

    EXPECT_EQ(channelId, reconstructedPacket->channelId());

    ASSERT_EQ(origPacket->getSerializedData().size(), reconstructedPacket->getSerializedData().size());
    for (size_t index = 0; index < origPacket->getSerializedData().size(); ++index)
        EXPECT_EQ(origPacket->getSerializedData()[index], reconstructedPacket->getSerializedData()[index]) << "mismatch at index " << index;
}

TEST(FidoHidMessageTest, TestMaxLengthPacketConstructors)
{
    uint32_t channelId = 0xAAABACAD;
    Vector<uint8_t> data;
    for (size_t i = 0; i < kHidMaxMessageSize; ++i)
        data.append(static_cast<uint8_t>(i % 0xff));

    auto origMsg = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kMsg, data);
    ASSERT_TRUE(origMsg);

    const auto& originalMsgPackets = origMsg->getPacketsForTesting();
    auto it = originalMsgPackets.begin();
    auto msgData = (*it)->getSerializedData();
    auto newMsg = FidoHidMessage::createFromSerializedData(msgData);
    ++it;

    for (; it != originalMsgPackets.end(); ++it) {
        msgData = (*it)->getSerializedData();
        EXPECT_TRUE(newMsg->addContinuationPacket(msgData));
    }

    auto origIt = originalMsgPackets.begin();
    const auto& newMsgPackets = newMsg->getPacketsForTesting();
    auto newMsgIt = newMsgPackets.begin();

    EXPECT_EQ(origMsg->numPackets(), newMsg->numPackets());
    for (; origIt != originalMsgPackets.end() || newMsgIt != newMsgPackets.end(); ++origIt, ++newMsgIt) {
        EXPECT_EQ((*origIt)->getPacketPayload(), (*newMsgIt)->getPacketPayload());

        EXPECT_EQ((*origIt)->channelId(), (*newMsgIt)->channelId());

        ASSERT_EQ((*origIt)->getSerializedData().size(), (*newMsgIt)->getSerializedData().size());
        for (size_t index = 0; index < (*origIt)->getSerializedData().size(); ++index)
            EXPECT_EQ((*origIt)->getSerializedData()[index], (*newMsgIt)->getSerializedData()[index]) << "mismatch at index " << index;
    }
}

TEST(FidoHidMessageTest, TestMessagePartitoning)
{
    uint32_t channelId = 0x01010203;
    Vector<uint8_t> data(kHidInitPacketDataSize + 1);
    auto twoPacketMessage = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kPing, data);
    ASSERT_TRUE(twoPacketMessage);
    EXPECT_EQ(2U, twoPacketMessage->numPackets());

    data.resize(kHidInitPacketDataSize);
    auto onePacketMessage = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kPing, data);
    ASSERT_TRUE(onePacketMessage);
    EXPECT_EQ(1U, onePacketMessage->numPackets());

    data.resize(kHidInitPacketDataSize + kHidContinuationPacketDataSize + 1);
    auto threePacketMessage = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kPing, data);
    ASSERT_TRUE(threePacketMessage);
    EXPECT_EQ(3U, threePacketMessage->numPackets());
}

TEST(FidoHidMessageTest, TestMaxSize)
{
    uint32_t channelId = 0x00010203;
    Vector<uint8_t> data(kHidMaxMessageSize + 1);
    auto oversizeMessage = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kPing, data);
    EXPECT_FALSE(oversizeMessage);
}

TEST(FidoHidMessageTest, TestDeconstruct)
{
    uint32_t channelId = 0x0A0B0C0D;
    Vector<uint8_t> data(kHidMaxMessageSize, 0x7F);
    auto filledMessage = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kPing, data);
    ASSERT_TRUE(filledMessage);
    EXPECT_EQ(data, filledMessage->getMessagePayload());
}

TEST(FidoHidMessageTest, TestDeserialize)
{
    uint32_t channelId = 0x0A0B0C0D;
    Vector<uint8_t> data(kHidMaxMessageSize);

    auto origMessage = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kPing, data);
    ASSERT_TRUE(origMessage);

    Deque<Vector<uint8_t>> origList;
    auto buf = origMessage->popNextPacket();
    origList.append(buf);

    auto newMessage = FidoHidMessage::createFromSerializedData(buf);
    while (!newMessage->messageComplete()) {
        buf = origMessage->popNextPacket();
        origList.append(buf);
        newMessage->addContinuationPacket(buf);
    }

    while (!(buf = newMessage->popNextPacket()).isEmpty()) {
        EXPECT_EQ(buf, origList.first());
        origList.removeFirst();
    }
}

TEST(FidoHidMessageTest, TestProperties)
{
    uint32_t channelId = 0x05060708;
    Vector<uint8_t> data;

    auto message = FidoHidMessage::create(channelId, FidoHidDeviceCommand::kCancel, data);
    EXPECT_EQ(channelId, message->channelId());
    EXPECT_EQ(FidoHidDeviceCommand::kCancel, message->cmd());
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
