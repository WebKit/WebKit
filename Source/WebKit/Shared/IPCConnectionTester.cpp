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
#include "IPCConnectionTester.h"

#if ENABLE(IPC_TESTING_API)
#include "IPCConnectionTesterMessages.h"
#include "IPCUtilities.h"

namespace WebKit {

Ref<IPCConnectionTester> IPCConnectionTester::create(IPC::Connection& connection, IPCConnectionTesterIdentifier identifier, IPC::Connection::Handle&& handle)
{
    auto tester = adoptRef(*new IPCConnectionTester(connection, identifier, WTFMove(handle)));
    tester->initialize();
    return tester;
}

IPCConnectionTester::IPCConnectionTester(Ref<IPC::Connection>&& connection, IPCConnectionTesterIdentifier identifier, IPC::Connection::Handle&& handle)
    : m_connection(WTFMove(connection))
    , m_testedConnection(IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(handle) }))
    , m_identifier(identifier)
{
}

IPCConnectionTester::~IPCConnectionTester() = default;

void IPCConnectionTester::initialize()
{
    m_testedConnection->open(*this);
}

void IPCConnectionTester::stopListeningForIPC(Ref<IPCConnectionTester>&& refFromConnection)
{
    m_testedConnection->invalidate();
}

void IPCConnectionTester::sendAsyncMessages(uint32_t messageCount)
{
    for (uint32_t i = 0; i < messageCount; ++i)
        m_testedConnection->send(Messages::IPCConnectionTester::AsyncMessage(i), 0);
}

void IPCConnectionTester::didClose(IPC::Connection&)
{
}

void IPCConnectionTester::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
    ASSERT_NOT_REACHED();
}

void IPCConnectionTester::asyncMessage(uint32_t value)
{
    if (m_previousAsyncMessageValue != value - 1) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    m_previousAsyncMessageValue = value;
}

void IPCConnectionTester::syncMessage(uint32_t value, CompletionHandler<void(uint32_t)>&& completionHandler)
{
    completionHandler(value + m_previousAsyncMessageValue);
}

}

#endif
