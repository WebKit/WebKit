/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include "PluginProcessConnectionManager.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "Decoder.h"
#include "Encoder.h"
#include "PluginProcessConnection.h"
#include "PluginProcessConnectionManagerMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"

#if OS(DARWIN) && !USE(UNIX_DOMAIN_SOCKETS)
#include "MachPort.h"
#endif

namespace WebKit {

Ref<PluginProcessConnectionManager> PluginProcessConnectionManager::create()
{
    return adoptRef(*new PluginProcessConnectionManager);
}

PluginProcessConnectionManager::PluginProcessConnectionManager()
    : m_queue(WorkQueue::create("com.apple.WebKit.PluginProcessConnectionManager"))
{
}

PluginProcessConnectionManager::~PluginProcessConnectionManager()
{
    ASSERT_NOT_REACHED();
}

void PluginProcessConnectionManager::initializeConnection(IPC::Connection* connection)
{
    connection->addWorkQueueMessageReceiver(Messages::PluginProcessConnectionManager::messageReceiverName(), m_queue.get(), this);
}

PluginProcessConnection* PluginProcessConnectionManager::getPluginProcessConnection(uint64_t pluginProcessToken)
{
    for (size_t i = 0; i < m_pluginProcessConnections.size(); ++i) {
        if (m_pluginProcessConnections[i]->pluginProcessToken() == pluginProcessToken)
            return m_pluginProcessConnections[i].get();
    }

    IPC::Attachment encodedConnectionIdentifier;
    bool supportsAsynchronousInitialization;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::GetPluginProcessConnection(pluginProcessToken),
                                                     Messages::WebProcessProxy::GetPluginProcessConnection::Reply(encodedConnectionIdentifier, supportsAsynchronousInitialization), 0))
        return nullptr;

#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::Identifier connectionIdentifier = encodedConnectionIdentifier.releaseFileDescriptor();
#elif OS(DARWIN)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
#elif OS(WINDOWS)
    IPC::Connection::Identifier connectionIdentifier = encodedConnectionIdentifier.handle();
#endif
    if (!IPC::Connection::identifierIsValid(connectionIdentifier))
        return nullptr;

    auto pluginProcessConnection = PluginProcessConnection::create(this, pluginProcessToken, connectionIdentifier, supportsAsynchronousInitialization);
    m_pluginProcessConnections.append(pluginProcessConnection.copyRef());

    {
        Locker locker { m_tokensAndConnectionsLock };
        ASSERT(!m_tokensAndConnections.contains(pluginProcessToken));

        m_tokensAndConnections.set(pluginProcessToken, pluginProcessConnection->connection());
    }

    return pluginProcessConnection.ptr();
}

void PluginProcessConnectionManager::removePluginProcessConnection(PluginProcessConnection* pluginProcessConnection)
{
    size_t vectorIndex = m_pluginProcessConnections.find(pluginProcessConnection);
    ASSERT(vectorIndex != notFound);

    {
        Locker locker { m_tokensAndConnectionsLock };
        ASSERT(m_tokensAndConnections.contains(pluginProcessConnection->pluginProcessToken()));
        
        m_tokensAndConnections.remove(pluginProcessConnection->pluginProcessToken());
    }

    m_pluginProcessConnections.remove(vectorIndex);
}

void PluginProcessConnectionManager::pluginProcessCrashed(uint64_t pluginProcessToken)
{
    Locker locker { m_tokensAndConnectionsLock };
    IPC::Connection* connection = m_tokensAndConnections.get(pluginProcessToken);

    // It's OK for connection to be null here; it will happen if this web process doesn't know
    // anything about the plug-in process.
    if (!connection)
        return;

    connection->postConnectionDidCloseOnConnectionWorkQueue();
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
