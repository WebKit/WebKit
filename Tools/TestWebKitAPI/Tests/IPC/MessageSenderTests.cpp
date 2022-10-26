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
#include "MessageSender.h"

#include "IPCTestUtilities.h"
#include "Test.h"

namespace TestWebKitAPI {

class MessageSenderTest : public ConnectionTestABBA {
};

namespace {

class SimpleMessageSender : public IPC::MessageSender {
public:
    SimpleMessageSender(IPC::Connection* connection)
        : m_connection(connection)
    {
    }

private:
    IPC::Connection* messageSenderConnection() const { return m_connection; };
    uint64_t messageSenderDestinationID() const { return 0u; };
    IPC::Connection* m_connection;
};

}

// Tests that async reply messages sent to a connection after invalidate()
// will receive the replies as cancelled.
TEST_P(MessageSenderTest, SendAsyncAfterInvalidateCancelsAllAsyncReplies)
{
    ASSERT_TRUE(openBoth());
    b()->invalidate();

    // This works for IPC::Connection.
    {
        HashSet<uint64_t> replies;
        for (uint64_t i = 100u; i < 160u; ++i) {
            b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, j = i] (uint64_t value) {
                EXPECT_EQ(value, 0u) << j;
                if (!value)
                    replies.add(j);
            }, i);
        }
        while (replies.size() < 60u)
            RunLoop::current().cycle();
    }

    // Same should work via IPC::MessageSender.
    {
        SimpleMessageSender sender { b() };
        HashSet<uint64_t> replies;
        for (uint64_t i = 100u; i < 160u; ++i) {
            sender.sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&, j = i] (uint64_t value) {
                EXPECT_EQ(value, 0u) << j;
                if (!value)
                    replies.add(j);
            }, i);
        }
        while (replies.size() < 60u)
            RunLoop::current().cycle();
    }
}

INSTANTIATE_TEST_SUITE_P(MessageSenderTest,
    MessageSenderTest,
    testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
    TestParametersToStringFormatter());

}
