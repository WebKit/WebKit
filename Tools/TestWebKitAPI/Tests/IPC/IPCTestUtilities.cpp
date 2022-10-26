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

#include "WTFStringUtilities.h" // NOLINT
#include "IPCTestUtilities.h" // NOLINT
#include <wtf/threads/BinarySemaphore.h> // NOLINT

namespace TestWebKitAPI {

void PrintTo(ConnectionTestDirection value, ::std::ostream* o)
{
    if (value == ConnectionTestDirection::ServerIsA)
        *o << "ServerIsA";
    else if (value == ConnectionTestDirection::ClientIsA)
        *o << "ClientIsA";
    else
        *o << "Unknown";
}

void ConnectionTestBase::setupBase()
{
    WTF::initializeMainThread();
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    if (!identifiers) {
        FAIL();
        return;
    }
    m_connections[0].connection = IPC::Connection::createServerConnection(WTFMove(identifiers->server));
    m_connections[1].connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { identifiers->client.leakSendRight() });
}

void ConnectionTestBase::teardownBase()
{
    for (auto& c : m_connections) {
        if (c.connection) {
            c.connection->invalidate();
            ensureConnectionWorkQueueEmpty(*c.connection);
        }
        c.connection = nullptr;
    }
    // Tests should handle all messages. Catch unexpected messages.
    {
        auto messages = aClient().takeMessages();
        EXPECT_EQ(messages.size(), 0u);
        for (uint64_t i = 0u; i < messages.size(); ++i) {
            SCOPED_TRACE(makeString("A had unexpected message: ", i));
            EXPECT_EQ(messages[i].messageName, static_cast<IPC::MessageName>(0xaaaa));
            EXPECT_EQ(messages[i].destinationID, 0xddddddu);
        }
    }
    {
        auto messages = bClient().takeMessages();
        EXPECT_EQ(messages.size(), 0u);
        for (uint64_t i = 0u; i < messages.size(); ++i) {
            SCOPED_TRACE(makeString("B had unexpected message message: ", i));
            EXPECT_EQ(messages[i].messageName, static_cast<IPC::MessageName>(0xaaaa));
            EXPECT_EQ(messages[i].destinationID, 0xddddddu);
        }
    }
}

void ConnectionTestBase::ensureConnectionWorkQueueEmpty(IPC::Connection& connection)
{
    // FIXME: currently we have no real way to ensure that a work queue completes.
    // On Cocoa this is a problem when exit() occurs while work queue is still cancelling
    // the dispatch queue sources.
    // To workaround for now, we dispatch one sync message to ensure invalidate is run to
    // cancel the dispatch sources and one sync message to ensure that the cancel handler
    // has run.
    for (int i = 0; i < 2; ++i) {
        BinarySemaphore semaphore;
        connection.dispatchOnReceiveQueueForTesting([&semaphore] {
            semaphore.signal();
        });
        semaphore.wait();
    }
}

}
