/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(PLUGIN_PROCESS)

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "PluginProcessConnection.h"
#include "PluginProcessConnectionManagerMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"

#if PLATFORM(MAC)
#include "MachPort.h"
#endif

namespace WebKit {

PluginProcessConnectionManager::PluginProcessConnectionManager()
{
}

PluginProcessConnectionManager::~PluginProcessConnectionManager()
{
}

PluginProcessConnection* PluginProcessConnectionManager::getPluginProcessConnection(const String& pluginPath, PluginProcess::Type processType)
{
    for (size_t i = 0; i < m_pluginProcessConnections.size(); ++i) {
        RefPtr<PluginProcessConnection>& pluginProcessConnection = m_pluginProcessConnections[i];
        if (pluginProcessConnection->pluginPath() == pluginPath && pluginProcessConnection->processType() == processType)
            return pluginProcessConnection.get();
    }

    CoreIPC::Attachment encodedConnectionIdentifier;
    bool supportsAsynchronousInitialization;
    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::GetPluginProcessConnection(pluginPath, processType),
                                                     Messages::WebProcessProxy::GetPluginProcessConnection::Reply(encodedConnectionIdentifier, supportsAsynchronousInitialization), 0))
        return 0;

#if PLATFORM(MAC)
    CoreIPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
    if (CoreIPC::Connection::identifierIsNull(connectionIdentifier))
        return 0;
#elif USE(UNIX_DOMAIN_SOCKETS)
    CoreIPC::Connection::Identifier connectionIdentifier = encodedConnectionIdentifier.fileDescriptor();
    if (connectionIdentifier == -1)
        return 0;
#endif

    RefPtr<PluginProcessConnection> pluginProcessConnection = PluginProcessConnection::create(this, pluginPath, processType, connectionIdentifier, supportsAsynchronousInitialization);
    m_pluginProcessConnections.append(pluginProcessConnection);

    {
        MutexLocker locker(m_pathsAndConnectionsMutex);
        ASSERT(!m_pathsAndConnections.contains(std::make_pair(pluginProcessConnection->pluginPath(), processType)));

        m_pathsAndConnections.set(std::make_pair(pluginPath, processType), pluginProcessConnection->connection());
    }

    return pluginProcessConnection.get();
}

void PluginProcessConnectionManager::removePluginProcessConnection(PluginProcessConnection* pluginProcessConnection)
{
    size_t vectorIndex = m_pluginProcessConnections.find(pluginProcessConnection);
    ASSERT(vectorIndex != notFound);

    {
        MutexLocker locker(m_pathsAndConnectionsMutex);
        ASSERT(m_pathsAndConnections.contains(std::make_pair(pluginProcessConnection->pluginPath(), pluginProcessConnection->processType())));
        
        m_pathsAndConnections.remove(std::make_pair(pluginProcessConnection->pluginPath(), pluginProcessConnection->processType()));
    }

    m_pluginProcessConnections.remove(vectorIndex);
}

void PluginProcessConnectionManager::didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection* connection, OwnPtr<CoreIPC::MessageDecoder>& decoder)
{
    if (decoder->messageReceiverName() == Messages::PluginProcessConnectionManager::messageReceiverName()) {
        didReceivePluginProcessConnectionManagerMessageOnConnectionWorkQueue(ChildProcess::connection(), decoder);
        return;
    }
}

void PluginProcessConnectionManager::didCloseOnConnectionWorkQueue(CoreIPC::Connection*)
{
}

void PluginProcessConnectionManager::pluginProcessCrashed(CoreIPC::Connection*, const String& pluginPath, uint32_t opaquePluginType)
{
    MutexLocker locker(m_pathsAndConnectionsMutex);
    CoreIPC::Connection* connection = m_pathsAndConnections.get(std::make_pair(pluginPath, static_cast<PluginProcess::Type>(opaquePluginType))).get();

    // It's OK for connection to be null here; it will happen if this web process doesn't know
    // anything about the plug-in process.
    if (!connection)
        return;

    connection->postConnectionDidCloseOnConnectionWorkQueue();
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
