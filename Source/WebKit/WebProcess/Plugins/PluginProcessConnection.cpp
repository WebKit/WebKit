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
#include "PluginProcessConnection.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NPObjectMessageReceiverMessages.h"
#include "NPRemoteObjectMap.h"
#include "NPRuntimeObjectMap.h"
#include "PluginProcessConnectionManager.h"
#include "PluginProxy.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <JavaScriptCore/JSObject.h>
#include <wtf/FileSystem.h>

namespace WebKit {

PluginProcessConnection::PluginProcessConnection(PluginProcessConnectionManager* pluginProcessConnectionManager, uint64_t pluginProcessToken, IPC::Connection::Identifier connectionIdentifier, bool supportsAsynchronousPluginInitialization)
    : m_pluginProcessConnectionManager(pluginProcessConnectionManager)
    , m_pluginProcessToken(pluginProcessToken)
    , m_supportsAsynchronousPluginInitialization(supportsAsynchronousPluginInitialization)
{
    m_connection = IPC::Connection::createClientConnection(connectionIdentifier, *this);

    m_npRemoteObjectMap = NPRemoteObjectMap::create(m_connection.get());

    m_connection->open();
}

PluginProcessConnection::~PluginProcessConnection()
{
    ASSERT(!m_connection);
    ASSERT(!m_npRemoteObjectMap);
}

void PluginProcessConnection::addPluginProxy(PluginProxy* plugin)
{
    ASSERT(!m_plugins.contains(plugin->pluginInstanceID()));
    m_plugins.set(plugin->pluginInstanceID(), plugin);
}

void PluginProcessConnection::removePluginProxy(PluginProxy* plugin)
{
    ASSERT(m_plugins.contains(plugin->pluginInstanceID()));
    m_plugins.remove(plugin->pluginInstanceID());

    // Invalidate all objects related to this plug-in.
    m_npRemoteObjectMap->pluginDestroyed(plugin);

    if (!m_plugins.isEmpty())
        return;

    m_npRemoteObjectMap = nullptr;

    // We have no more plug-ins, invalidate the connection to the plug-in process.
    ASSERT(m_connection);
    m_connection->invalidate();
    m_connection = nullptr;

    // This will cause us to be deleted.
    m_pluginProcessConnectionManager->removePluginProcessConnection(this);
}

void PluginProcessConnection::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.destinationID());

    PluginProxy* pluginProxy = m_plugins.get(decoder.destinationID());
    if (!pluginProxy)
        return;

    pluginProxy->didReceivePluginProxyMessage(connection, decoder);
}

bool PluginProcessConnection::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    if (decoder.messageReceiverName() == Messages::NPObjectMessageReceiver::messageReceiverName())
        return m_npRemoteObjectMap->didReceiveSyncMessage(connection, decoder, replyEncoder);

    uint64_t destinationID = decoder.destinationID();

    if (!destinationID)
        return didReceiveSyncPluginProcessConnectionMessage(connection, decoder, replyEncoder);

    PluginProxy* pluginProxy = m_plugins.get(destinationID);
    if (!pluginProxy)
        return false;

    return pluginProxy->didReceiveSyncPluginProxyMessage(connection, decoder, replyEncoder);
}

void PluginProcessConnection::didClose(IPC::Connection&)
{
    // The plug-in process must have crashed.
    for (HashMap<uint64_t, PluginProxy*>::const_iterator::Values it = m_plugins.begin().values(), end = m_plugins.end().values(); it != end; ++it) {
        PluginProxy* pluginProxy = (*it);

        pluginProxy->pluginProcessCrashed();
    }
}

void PluginProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
}

void PluginProcessConnection::setException(const String& exceptionString, CompletionHandler<void()>&& completionHandler)
{
    NPRuntimeObjectMap::setGlobalException(exceptionString);
    completionHandler();
}
    
} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
