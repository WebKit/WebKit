/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "StreamClientConnectionBuffer.h"
#include "StreamServerConnectionBuffer.h"
#include "Test.h"

namespace TestWebKitAPI {

TEST(StreamConnectionBufferTests, CreateWorks)
{
    IPC::StreamClientConnectionBuffer b(8);
    EXPECT_NE(b.data(), nullptr);
    EXPECT_EQ(b.dataSize(), 256u);
    {
        auto server = IPC::StreamServerConnectionBuffer::map(b.createHandle());
        ASSERT_TRUE(server.has_value());
        EXPECT_EQ(b.dataSize(), server->dataSize());
    }
    IPC::StreamClientConnectionBuffer b2(24);
    EXPECT_NE(b2.data(), nullptr);
    EXPECT_EQ(b2.dataSize(), 16777216u);
    {
        auto server = IPC::StreamServerConnectionBuffer::map(b2.createHandle());
        ASSERT_TRUE(server.has_value());
        EXPECT_EQ(b2.dataSize(), server->dataSize());
    }
}

class StreamConnectionBufferTest : public ::testing::TestWithParam<std::tuple<unsigned>> {
public:
    unsigned bufferSizeLog2() const
    {
        return std::get<0>(GetParam());
    }

    void SetUp() override
    {
        m_client.emplace(bufferSizeLog2());
        m_maybeServer = IPC::StreamServerConnectionBuffer::map(m_client->createHandle());
    }

    IPC::StreamClientConnectionBuffer& client() { return *m_client; }
    IPC::StreamServerConnectionBuffer& server() { return *m_maybeServer; }
    bool isValid() const { return m_maybeServer.has_value(); }

protected:
    static constexpr size_t minimumSize = IPC::StreamConnectionEncoder::minimumMessageSize;
    std::optional<IPC::StreamClientConnectionBuffer> m_client;
    std::optional<IPC::StreamServerConnectionBuffer> m_maybeServer;
};

static void fill(Span<uint8_t> span)
{
    for (size_t i = 0; i < span.size(); ++i)
        span[i] = i;
}

static void expectFilled(Span<uint8_t> span)
{
    size_t reportFailureCount = 5;
    for (size_t i = 0; i < span.size() && reportFailureCount; ++i) {
        EXPECT_EQ(static_cast<uint8_t>(i), span[i]);
        if (span[i] != static_cast<uint8_t>(i))
            reportFailureCount--;
    }
}

TEST_P(StreamConnectionBufferTest, ClientStartsFull)
{
    ASSERT_TRUE(isValid());
    auto span = client().tryAcquire(0_s);
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->size(), client().dataSize() - 1);
}

TEST_P(StreamConnectionBufferTest, ServerStartsEmpty)
{
    ASSERT_TRUE(isValid());
    auto span = server().tryAcquire();
    EXPECT_FALSE(span.has_value());
}

TEST_P(StreamConnectionBufferTest, ClientWritesFullVisibleInServer)
{
    ASSERT_TRUE(isValid());
    size_t expectedServerSize;
    {
        auto span = client().tryAcquire(0_s);
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), client().dataSize() - 1);
        fill(*span);
        client().release(span->size());
        expectedServerSize = span->size();

        span = client().tryAcquire(0_s);
        ASSERT_FALSE(span.has_value());
    }
    auto serverSpan = server().tryAcquire();
    ASSERT_EQ(expectedServerSize, serverSpan->size());
    expectFilled(*serverSpan);
}

INSTANTIATE_TEST_SUITE_P(StreamConnectionBufferSizedTests,
    StreamConnectionBufferTest,
    testing::Values(6, 9, 14),
    TestParametersToStringFormatter());

}
