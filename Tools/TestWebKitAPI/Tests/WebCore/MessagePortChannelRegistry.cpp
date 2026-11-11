/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/MessagePortChannel.h>
#include <WebCore/MessagePortChannelRegistry.h>
#include <WebCore/MessageWithMessagePorts.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/MainThread.h>

namespace TestWebKitAPI {

using namespace WebCore;

static MessagePortIdentifier makePort(ProcessIdentifier process)
{
    return { process, PortIdentifier::generate() };
}

static MessageWithMessagePorts messageCarrying(const MessagePortIdentifier& port, const MessagePortIdentifier& remote)
{
    return { nullptr, { { port, remote } } };
}

static void expectTransferredPortClosed(MessagePortChannelRegistry& registry, const MessagePortIdentifier& transferredPort, const MessagePortIdentifier& remainingPort)
{
    RefPtr channel = registry.existingChannelContainingPort(remainingPort);
    ASSERT_TRUE(channel);

    EXPECT_FALSE(registry.didPostMessageToRemote({ nullptr, { } }, transferredPort));
    EXPECT_FALSE(channel->hasAnyMessagesPendingOrInFlight());

    registry.didCloseMessagePort(remainingPort, MessagePortStatus::Closed);
    channel = nullptr;
    EXPECT_FALSE(registry.existingChannelContainingPort(remainingPort));

    registry.didCloseMessagePort(transferredPort, MessagePortStatus::Closed);
}

TEST(MessagePortChannelRegistry, MessageToClosedPortClosesTransferredPorts)
{
    WTF::initializeMainThread();
    MessagePortChannelRegistry registry;
    auto process = ProcessIdentifier::generate();

    auto a1 = makePort(process);
    auto a2 = makePort(process);
    auto b1 = makePort(process);
    auto b2 = makePort(process);
    registry.didCreateMessagePortChannel(a1, a2);
    registry.didCreateMessagePortChannel(b1, b2);

    registry.didCloseMessagePort(a2, MessagePortStatus::Closed);
    registry.didDisentangleMessagePort(b2);
    EXPECT_FALSE(registry.didPostMessageToRemote(messageCarrying(b2, b1), a2));

    expectTransferredPortClosed(registry, b2, b1);
    registry.didCloseMessagePort(a1, MessagePortStatus::Closed);
}

TEST(MessagePortChannelRegistry, MessageToDestroyedChannelClosesTransferredPorts)
{
    WTF::initializeMainThread();
    MessagePortChannelRegistry registry;
    auto process = ProcessIdentifier::generate();

    auto a1 = makePort(process);
    auto a2 = makePort(process);
    auto b1 = makePort(process);
    auto b2 = makePort(process);
    registry.didCreateMessagePortChannel(a1, a2);
    registry.didCreateMessagePortChannel(b1, b2);

    registry.didCloseMessagePort(a1, MessagePortStatus::Closed);
    registry.didCloseMessagePort(a2, MessagePortStatus::Closed);
    ASSERT_FALSE(registry.existingChannelContainingPort(a2));

    registry.didDisentangleMessagePort(b2);
    EXPECT_FALSE(registry.didPostMessageToRemote(messageCarrying(b2, b1), a2));

    expectTransferredPortClosed(registry, b2, b1);
}

TEST(MessagePortChannelRegistry, ClosingPortWithPendingMessagesClosesTransferredPorts)
{
    WTF::initializeMainThread();
    MessagePortChannelRegistry registry;
    auto process = ProcessIdentifier::generate();

    auto a1 = makePort(process);
    auto a2 = makePort(process);
    auto b1 = makePort(process);
    auto b2 = makePort(process);
    registry.didCreateMessagePortChannel(a1, a2);
    registry.didCreateMessagePortChannel(b1, b2);

    registry.didDisentangleMessagePort(b2);
    EXPECT_TRUE(registry.didPostMessageToRemote(messageCarrying(b2, b1), a2));
    registry.didCloseMessagePort(a2, MessagePortStatus::Closed);

    expectTransferredPortClosed(registry, b2, b1);
    registry.didCloseMessagePort(a1, MessagePortStatus::Closed);
}

TEST(MessagePortChannelRegistry, DeliveredMessageKeepsTransferredPortsOpen)
{
    WTF::initializeMainThread();
    MessagePortChannelRegistry registry;
    auto process = ProcessIdentifier::generate();

    auto a1 = makePort(process);
    auto a2 = makePort(process);
    auto b1 = makePort(process);
    auto b2 = makePort(process);
    registry.didCreateMessagePortChannel(a1, a2);
    registry.didCreateMessagePortChannel(b1, b2);

    registry.didDisentangleMessagePort(b2);
    EXPECT_TRUE(registry.didPostMessageToRemote(messageCarrying(b2, b1), a2));

    Vector<MessageWithMessagePorts> received;
    registry.takeAllMessagesForPort(a2, [&](Vector<MessageWithMessagePorts>&& messages, CompletionHandler<void()>&& completionHandler) {
        received = WTF::move(messages);
        completionHandler();
    });
    ASSERT_EQ(received.size(), 1u);
    ASSERT_EQ(received[0].transferredPorts.size(), 1u);
    EXPECT_EQ(received[0].transferredPorts[0].first, b2);

    registry.didEntangleLocalToRemote(b2, b1, process);
    EXPECT_TRUE(registry.didPostMessageToRemote({ nullptr, { } }, b2));

    registry.didCloseMessagePort(a1, MessagePortStatus::Closed);
    registry.didCloseMessagePort(a2, MessagePortStatus::Closed);
    registry.didCloseMessagePort(b1, MessagePortStatus::Closed);
    registry.didCloseMessagePort(b2, MessagePortStatus::Closed);
    EXPECT_FALSE(registry.existingChannelContainingPort(b1));
}

} // namespace TestWebKitAPI
