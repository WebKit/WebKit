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

#include <WebCore/RunLoop.h>

using namespace WebCore;

namespace WebKit {

ChildProcessProxy::ChildProcessProxy(CoreIPC::Connection::QueueClient* queueClient)
    : m_queueClient(queueClient)
{
}

ChildProcessProxy::~ChildProcessProxy()
{
    if (m_connection)
        m_connection->invalidate();

    for (size_t i = 0; i < m_pendingMessages.size(); ++i)
        m_pendingMessages[i].first.releaseArguments();

    if (m_processLauncher) {
        m_processLauncher->invalidate();
        m_processLauncher = 0;
    }
}

ChildProcessProxy* ChildProcessProxy::fromConnection(CoreIPC::Connection* connection)
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

bool ChildProcessProxy::sendMessage(CoreIPC::MessageID messageID, PassOwnPtr<CoreIPC::MessageEncoder> encoder, unsigned messageSendFlags)
{
    // If we're waiting for the web process to launch, we need to stash away the messages so we can send them once we have
    // a CoreIPC connection.
    if (isLaunching()) {
        m_pendingMessages.append(std::make_pair(CoreIPC::Connection::OutgoingMessage(messageID, encoder), messageSendFlags));
        return true;
    }

    // If the web process has exited, connection will be null here.
    if (!connection())
        return false;

    return connection()->sendMessage(messageID, encoder, messageSendFlags);
}

bool ChildProcessProxy::isLaunching() const
{
    if (m_processLauncher)
        return m_processLauncher->isLaunching();

    return false;
}

void ChildProcessProxy::didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);

    m_connection = CoreIPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
#if OS(DARWIN)
    m_connection->setShouldCloseConnectionOnMachExceptions();
#elif PLATFORM(QT) && !OS(WINDOWS)
    m_connection->setShouldCloseConnectionOnProcessTermination(processIdentifier());
#endif

    if (m_queueClient)
        m_connection->addQueueClient(m_queueClient);
    m_connection->open();

    for (size_t i = 0; i < m_pendingMessages.size(); ++i) {
        CoreIPC::Connection::OutgoingMessage& outgoingMessage = m_pendingMessages[i].first;
        unsigned messageSendFlags = m_pendingMessages[i].second;
        m_connection->sendMessage(outgoingMessage.messageID(), adoptPtr(outgoingMessage.arguments()), messageSendFlags);
    }

    m_pendingMessages.clear();
}

void ChildProcessProxy::clearConnection()
{
    if (!m_connection)
        return;

    if (m_queueClient)
        m_connection->removeQueueClient(m_queueClient);
    m_connection->invalidate();
    m_connection = nullptr;
}

}
