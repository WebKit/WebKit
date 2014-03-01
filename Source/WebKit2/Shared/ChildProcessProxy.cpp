/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ChildProcessProxy.h"

#include <wtf/RunLoop.h>

namespace WebKit {

ChildProcessProxy::ChildProcessProxy()
{
}

ChildProcessProxy::~ChildProcessProxy()
{
    if (m_connection)
        m_connection->invalidate();

    if (m_processLauncher) {
        m_processLauncher->invalidate();
        m_processLauncher = 0;
    }
}

ChildProcessProxy* ChildProcessProxy::fromConnection(IPC::Connection* connection)
{
    ASSERT(connection);

    ChildProcessProxy* childProcessProxy = static_cast<ChildProcessProxy*>(connection->client());
    ASSERT(childProcessProxy->connection() == connection);

    return childProcessProxy;
}

void ChildProcessProxy::connect()
{
    ASSERT(!m_processLauncher);
    ProcessLauncher::LaunchOptions launchOptions;
    getLaunchOptions(launchOptions);
    m_processLauncher = ProcessLauncher::create(this, launchOptions);
}

void ChildProcessProxy::terminate()
{
    if (m_processLauncher)
        m_processLauncher->terminateProcess();
}

ChildProcessProxy::State ChildProcessProxy::state() const
{
    if (m_processLauncher && m_processLauncher->isLaunching())
        return ChildProcessProxy::State::Launching;

    if (!m_connection)
        return ChildProcessProxy::State::Terminated;

    return ChildProcessProxy::State::Running;
}

bool ChildProcessProxy::sendMessage(std::unique_ptr<IPC::MessageEncoder> encoder, unsigned messageSendFlags)
{
    switch (state()) {
    case State::Launching:
        // If we're waiting for the child process to launch, we need to stash away the messages so we can send them once we have a connection.
        m_pendingMessages.append(std::make_pair(std::move(encoder), messageSendFlags));
        return true;

    case State::Running:
        return connection()->sendMessage(std::move(encoder), messageSendFlags);

    case State::Terminated:
        return false;
    }

    return false;
}

void ChildProcessProxy::addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void ChildProcessProxy::addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void ChildProcessProxy::removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

bool ChildProcessProxy::dispatchMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
{
    return m_messageReceiverMap.dispatchMessage(connection, decoder);
}

bool ChildProcessProxy::dispatchSyncMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, decoder, replyEncoder);
}

void ChildProcessProxy::didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);

    m_connection = IPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
#if !PLATFORM(IOS)
    m_connection->setShouldCloseConnectionOnMachExceptions();
#endif

    connectionWillOpen(m_connection.get());
    m_connection->open();

    for (size_t i = 0; i < m_pendingMessages.size(); ++i) {
        std::unique_ptr<IPC::MessageEncoder> message = std::move(m_pendingMessages[i].first);
        unsigned messageSendFlags = m_pendingMessages[i].second;
        m_connection->sendMessage(std::move(message), messageSendFlags);
    }

    m_pendingMessages.clear();
}

void ChildProcessProxy::abortProcessLaunchIfNeeded()
{
    if (state() != State::Launching)
        return;

    m_processLauncher->invalidate();
    m_processLauncher = nullptr;
}

void ChildProcessProxy::clearConnection()
{
    if (!m_connection)
        return;

    connectionWillClose(m_connection.get());

    m_connection->invalidate();
    m_connection = nullptr;
}

void ChildProcessProxy::connectionWillOpen(IPC::Connection*)
{
}

void ChildProcessProxy::connectionWillClose(IPC::Connection*)
{
}

} // namespace WebKit
