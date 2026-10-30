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
#include "IPCTestUtilities.h"

#include "Helpers/Test.h"
#include <wtf/text/MakeString.h>
#include <wtf/threads/BinarySemaphore.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(IPC_SEND_DIRECTLY)
#include <pthread/qos.h>
#endif

namespace TestWebKitAPI {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MockConnectionClient);

void PrintTo(ConnectionTestDirection value, ::std::ostream* o)
{
    if (value == ConnectionTestDirection::ServerIsA)
        *o << "ServerIsA";
    else if (value == ConnectionTestDirection::ClientIsA)
        *o << "ClientIsA";
    else
        *o << "Unknown";
}

void PrintTo(ConnectionTestDirectSend value, ::std::ostream* o)
{
    switch (value) {
    case ConnectionTestDirectSend::Disabled:
        *o << "DirectSendDisabled";
        return;
    case ConnectionTestDirectSend::Enabled:
        *o << "DirectSendEnabled";
        return;
    }
    *o << "Unknown";
}

std::string ConnectionTestABBAParametersToStringFormatter::operator()(const testing::TestParamInfo<ConnectionTestABBA::ParamType>& info) const
{
    auto name = testing::PrintToString(std::get<0>(info.param));
    if (std::get<1>(info.param) != ConnectionTestDirectSend::Disabled)
        name += testing::PrintToString(std::get<1>(info.param));
    return name;
}

ConnectionTestDirectSendScope::ConnectionTestDirectSendScope(ConnectionTestDirectSend directSend)
{
#if ENABLE(IPC_SEND_DIRECTLY)
    m_savedAllowsSendingDirectly = IPC::Connection::allowsSendingDirectly();
    IPC::Connection::setAllowsSendingDirectly(directSend == ConnectionTestDirectSend::Enabled);
    if (directSend == ConnectionTestDirectSend::Disabled)
        return;
    // Messages are only sent directly from threads whose QoS is at least the connection queue's. If this fails, the
    // tests would pass without sending anything directly.
    m_savedQOSClass = qos_class_self();
    EXPECT_EQ(pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0), 0);
    EXPECT_EQ(qos_class_self(), QOS_CLASS_USER_INTERACTIVE);
#else
    ASSERT_UNUSED(directSend, directSend == ConnectionTestDirectSend::Disabled);
#endif
}

ConnectionTestDirectSendScope::~ConnectionTestDirectSendScope()
{
#if ENABLE(IPC_SEND_DIRECTLY)
    IPC::Connection::setAllowsSendingDirectly(m_savedAllowsSendingDirectly);
    if (!m_savedQOSClass)
        return;
    // A thread can't go back to having no QoS class, so give it the QoS the main thread started with instead, or the
    // default QoS.
    auto qosClass = *m_savedQOSClass;
    if (qosClass == QOS_CLASS_UNSPECIFIED)
        qosClass = qos_class_main();
    if (qosClass == QOS_CLASS_UNSPECIFIED)
        qosClass = QOS_CLASS_DEFAULT;
    EXPECT_EQ(pthread_set_qos_class_self_np(qosClass, 0), 0);
#endif
}

void ConnectionTestBase::setupBase()
{
    WTF::initializeMainThread();
    auto identifiers = IPC::Connection::createConnectionIdentifierPair();
    if (!identifiers) {
        FAIL();
        return;
    }
    m_connections[0].connection = IPC::Connection::createServerConnection(WTF::move(identifiers->server));
    m_connections[1].connection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(identifiers->client) });
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
            SCOPED_TRACE(makeString("A had unexpected message: "_s, i));
            EXPECT_EQ(messages[i].messageName, static_cast<IPC::MessageName>(0xaaaa));
            EXPECT_EQ(messages[i].destinationID, 0xddddddu);
        }
    }
    {
        auto messages = bClient().takeMessages();
        EXPECT_EQ(messages.size(), 0u);
        for (uint64_t i = 0u; i < messages.size(); ++i) {
            SCOPED_TRACE(makeString("B had unexpected message message: "_s, i));
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

void PrintTo(InvalidMessageTestType value, ::std::ostream* o)
{
    switch (value) {
    case InvalidMessageTestType::DecodeError:
        *o << "DecodeError";
        return;
    case InvalidMessageTestType::ValidationError:
        *o << "ValidationError";
        return;
    default:
        break;
    }
    *o << "Unknown";
}


}
