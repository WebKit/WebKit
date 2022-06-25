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

#include <WebCore/ApduCommand.h>
#include <WebCore/ApduResponse.h>

namespace TestWebKitAPI {

using namespace apdu;

TEST(ApduTest, TestDeserializeBasic)
{
    uint8_t cla = 0xAA;
    uint8_t ins = 0xAB;
    uint8_t p1 = 0xAC;
    uint8_t p2 = 0xAD;
    Vector<uint8_t> message({ cla, ins, p1, p2 });
    auto cmd = ApduCommand::createFromMessage(message);
    ASSERT_TRUE(cmd);
    EXPECT_EQ(0u, cmd->responseLength());
    EXPECT_TRUE(cmd->data().isEmpty());
    EXPECT_EQ(cla, cmd->cla());
    EXPECT_EQ(ins, cmd->ins());
    EXPECT_EQ(p1, cmd->p1());
    EXPECT_EQ(p2, cmd->p2());
    // Invalid length.
    message = { cla, ins, p1 };
    EXPECT_FALSE(ApduCommand::createFromMessage(message));
    message.append(p2);
    message.append(0);
    // Set APDU command data size as maximum.
    message.append(0xFF);
    message.append(0xFF);
    message.resize(message.size() + ApduCommand::kApduMaxDataLength);
    // Set maximum response size.
    message.append(0);
    message.append(0);
    // |message| is APDU encoded byte array with maximum data length.
    EXPECT_TRUE(ApduCommand::createFromMessage(message));
    message.append(0);
    // |message| encoding containing data of size  maximum data length + 1.
    EXPECT_FALSE(ApduCommand::createFromMessage(message));
}

TEST(ApduTest, TestDeserializeComplex)
{
    uint8_t cla = 0xAA;
    uint8_t ins = 0xAB;
    uint8_t p1 = 0xAC;
    uint8_t p2 = 0xAD;
    Vector<uint8_t> data(ApduCommand::kApduMaxDataLength - ApduCommand::kApduMaxHeader - 2, 0x7F);
    Vector<uint8_t> message = { cla, ins, p1, p2, 0 };
    message.append((data.size() >> 8) & 0xff);
    message.append(data.size() & 0xff);
    message.appendVector(data);

    // Create a message with no response expected.
    auto cmdNoResponse = ApduCommand::createFromMessage(message);
    ASSERT_TRUE(cmdNoResponse);
    EXPECT_EQ(0u, cmdNoResponse->responseLength());
    EXPECT_EQ(data, cmdNoResponse->data());
    EXPECT_EQ(cla, cmdNoResponse->cla());
    EXPECT_EQ(ins, cmdNoResponse->ins());
    EXPECT_EQ(p1, cmdNoResponse->p1());
    EXPECT_EQ(p2, cmdNoResponse->p2());

    // Add response length to message.
    message.append(0xF1);
    message.append(0xD0);
    auto cmd = ApduCommand::createFromMessage(message);
    ASSERT_TRUE(cmd);
    EXPECT_EQ(data, cmd->data());
    EXPECT_EQ(cla, cmd->cla());
    EXPECT_EQ(ins, cmd->ins());
    EXPECT_EQ(p1, cmd->p1());
    EXPECT_EQ(p2, cmd->p2());
    EXPECT_EQ(static_cast<size_t>(0xF1D0), cmd->responseLength());
}

TEST(ApduTest, TestDeserializeResponse)
{
    ApduResponse::Status status;
    Vector<uint8_t> testVector;
    // Invalid length.
    Vector<uint8_t> message({ 0xAA });
    EXPECT_FALSE(ApduResponse::createFromMessage(message));
    // Valid length and status.
    status = ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED;
    message = { static_cast<uint8_t>(static_cast<uint16_t>(status) >> 8), static_cast<uint8_t>(status) };
    auto response = ApduResponse::createFromMessage(message);
    ASSERT_TRUE(response);
    EXPECT_EQ(ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED, response->status());
    EXPECT_EQ(response->data(), Vector<uint8_t>());
    // Valid length and status.
    status = ApduResponse::Status::SW_NO_ERROR;
    message = { static_cast<uint8_t>(static_cast<uint16_t>(status) >> 8), static_cast<uint8_t>(status)};
    testVector = { 0x01, 0x02, 0xEF, 0xFF };
    message.insertVector(0, testVector);
    response = ApduResponse::createFromMessage(message);
    ASSERT_TRUE(response);
    EXPECT_EQ(ApduResponse::Status::SW_NO_ERROR, response->status());
    EXPECT_EQ(response->data(), testVector);
}

TEST(ApduTest, TestSerializeCommand)
{
    ApduCommand cmd;
    cmd.setCla(0xA);
    cmd.setIns(0xB);
    cmd.setP1(0xC);
    cmd.setP2(0xD);
    // No data, no response expected.
    Vector<uint8_t> expected({ 0xA, 0xB, 0xC, 0xD });
    ASSERT(expected == cmd.getEncodedCommand());
    auto deserializedCmd = ApduCommand::createFromMessage(expected);
    ASSERT_TRUE(deserializedCmd);
    EXPECT_EQ(expected, deserializedCmd->getEncodedCommand());
    // No data, response expected.
    cmd.setResponseLength(0xCAFE);
    expected = { 0xA, 0xB, 0xC, 0xD, 0x0, 0xCA, 0xFE };
    EXPECT_EQ(expected, cmd.getEncodedCommand());
    deserializedCmd = ApduCommand::createFromMessage(expected);
    ASSERT_TRUE(deserializedCmd);
    EXPECT_EQ(expected, deserializedCmd->getEncodedCommand());
    // Data exists, response expected.
    Vector<uint8_t> data({ 0x1, 0x2, 0x3, 0x4 });
    cmd.setData(WTFMove(data));
    expected = { 0xA, 0xB, 0xC, 0xD, 0x0,  0x0, 0x4, 0x1, 0x2, 0x3, 0x4, 0xCA, 0xFE };
    EXPECT_EQ(expected, cmd.getEncodedCommand());
    deserializedCmd = ApduCommand::createFromMessage(expected);
    ASSERT_TRUE(deserializedCmd);
    EXPECT_EQ(expected, deserializedCmd->getEncodedCommand());
    // Data exists, no response expected.
    cmd.setResponseLength(0);
    expected = { 0xA, 0xB, 0xC, 0xD, 0x0, 0x0, 0x4, 0x1, 0x2, 0x3, 0x4 };
    EXPECT_EQ(expected, cmd.getEncodedCommand());
    EXPECT_EQ(expected, ApduCommand::createFromMessage(expected)->getEncodedCommand());
}

TEST(ApduTest, TestSerializeEdgeCases)
{
    ApduCommand cmd;
    cmd.setCla(0xA);
    cmd.setIns(0xB);
    cmd.setP1(0xC);
    cmd.setP2(0xD);
    // Set response length to maximum, which should serialize to 0x0000.
    cmd.setResponseLength(ApduCommand::kApduMaxResponseLength);
    Vector<uint8_t> expected({ 0xA, 0xB, 0xC, 0xD, 0x0, 0x0, 0x0 });
    EXPECT_EQ(expected, cmd.getEncodedCommand());
    auto deserializedCmd = ApduCommand::createFromMessage(expected);
    ASSERT_TRUE(deserializedCmd);
    EXPECT_EQ(expected, deserializedCmd->getEncodedCommand());
    // Maximum data size.
    Vector<uint8_t> oversized(ApduCommand::kApduMaxDataLength);
    cmd.setData(WTFMove(oversized));
    deserializedCmd = ApduCommand::createFromMessage(cmd.getEncodedCommand());
    ASSERT_TRUE(deserializedCmd);
    EXPECT_EQ(cmd.getEncodedCommand(), deserializedCmd->getEncodedCommand());
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
