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

#include "IPCEvent.h"
#include "StreamClientConnectionBuffer.h"
#include "StreamServerConnectionBuffer.h"
#include "Helpers/Test.h"

namespace TestWebKitAPI {

// The client waits on an Event for ring buffer backpressure, and the matching Signal normally
// travels to the server in StreamServerConnectionHandle. These tests exercise the buffer on its
// own, so they hold on to the Signal to keep the Event's sender alive.
struct ClientBuffer {
    std::optional<IPC::Signal> signal;
    std::optional<IPC::StreamClientConnectionBuffer> buffer;
};

static ClientBuffer createClientBuffer(unsigned dataSizeLog2)
{
    auto pair = IPC::createEventSignalPair();
    if (!pair)
        return { };
    return { WTF::move(pair->signal), IPC::StreamClientConnectionBuffer::create(dataSizeLog2, WTF::move(pair->event)) };
}

TEST(StreamConnectionBufferTests, CreateWorks)
{
    auto client = createClientBuffer(8);
    ASSERT_TRUE(client.buffer.has_value());
    auto& b = *client.buffer;
    EXPECT_NE(b.span().data(), nullptr);
    EXPECT_EQ(b.dataSize(), 256u);
    {
        auto server = IPC::StreamServerConnectionBuffer::map(b.createHandle());
        ASSERT_TRUE(server.has_value());
        EXPECT_EQ(b.dataSize(), server->dataSize());
    }
    auto client2 = createClientBuffer(24);
    ASSERT_TRUE(client2.buffer.has_value());
    auto& b2 = *client2.buffer;
    EXPECT_NE(b2.span().data(), nullptr);
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
        m_client = createClientBuffer(bufferSizeLog2());
        ASSERT(m_client.buffer);
        m_maybeServer = IPC::StreamServerConnectionBuffer::map(m_client.buffer->createHandle());
    }

    IPC::StreamClientConnectionBuffer& client() { return *m_client.buffer; }
    IPC::StreamServerConnectionBuffer& server() { return *m_maybeServer; }
    bool isValid() const { return m_maybeServer.has_value(); }

protected:
    static constexpr size_t minimumSize = IPC::StreamConnectionEncoder::minimumMessageSize;
    ClientBuffer m_client;
    std::optional<IPC::StreamServerConnectionBuffer> m_maybeServer;
};

static void fill(std::span<uint8_t> span)
{
    for (size_t i = 0; i < span.size(); ++i)
        span[i] = i;
}

static void expectFilled(std::span<uint8_t> span)
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

TEST_P(StreamConnectionBufferTest, ServerDoesNotReadPastReleaseAll)
{
    ASSERT_TRUE(isValid());
    {
        auto span = client().tryAcquire(0_s);
        ASSERT_TRUE(span.has_value());
        fill(span->first(3 * minimumSize));
        client().release(3 * minimumSize);
    }
    {
        auto span = server().tryAcquire();
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), 3 * minimumSize);
        server().release(minimumSize);
        server().releaseAll();
    }
    EXPECT_FALSE(server().tryAcquire().has_value());

    auto replySpan = client().tryAcquireAll(0_s);
    ASSERT_TRUE(replySpan.has_value());
    {
        auto span = client().tryAcquire(0_s);
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), client().dataSize() - 1);
        fill(span->first(minimumSize));
        EXPECT_EQ(client().release(minimumSize), IPC::StreamClientConnectionBuffer::WakeUpServer::Yes);
    }
    auto span = server().tryAcquire();
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->size(), minimumSize);
    expectFilled(*span);
}

TEST_P(StreamConnectionBufferTest, ClientSeesSpaceFreedAfterItsKnownSpaceRunsOut)
{
    ASSERT_TRUE(isValid());
    size_t half = client().dataSize() / 2;
    {
        auto span = client().tryAcquire(0_s);
        ASSERT_TRUE(span.has_value());
        fill(span->first(half));
        client().release(half);
    }
    {
        auto span = client().acquireNoWait();
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), half - 1);
        client().release(span->size());
    }
    EXPECT_FALSE(client().acquireNoWait().has_value());
    {
        auto span = server().tryAcquire();
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), client().dataSize() - 1);
        expectFilled(span->first(half));
        server().release(half);
    }
    auto span = client().acquireNoWait();
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->size(), half - 1);
}

TEST_P(StreamConnectionBufferTest, ClientAcquireLargerNoWaitRereadsServerOffset)
{
    ASSERT_TRUE(isValid());
    size_t quarter = client().dataSize() / 4;
    {
        auto span = client().tryAcquire(0_s);
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), client().dataSize() - 1);
        fill(*span);
        client().release(span->size());
    }
    {
        auto span = server().tryAcquire();
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), client().dataSize() - 1);
        expectFilled(*span);
        server().release(2 * quarter);
    }
    // The client wraps to offset 0 and reads the server offset, 2 * quarter.
    {
        auto span = client().acquireNoWait();
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), 2 * quarter - 1);
    }
    // The server frees another quarter. The client does not read the server offset again while its known span holds a message.
    server().release(quarter);
    {
        auto span = client().acquireNoWait();
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), 2 * quarter - 1);
    }
    // A message larger than the known span: reading the server offset again shows the space the server freed.
    {
        auto span = client().acquireLargerNoWait(2 * quarter - 1);
        ASSERT_TRUE(span.has_value());
        EXPECT_EQ(span->size(), 3 * quarter - 1);
    }
    EXPECT_FALSE(client().acquireLargerNoWait(3 * quarter - 1).has_value());
    auto span = client().acquireNoWait();
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->size(), 3 * quarter - 1);
}

INSTANTIATE_TEST_SUITE_P(StreamConnectionBufferSizedTests,
    StreamConnectionBufferTest,
    testing::Values(6, 9, 14),
    TestParametersToStringFormatter());

}
