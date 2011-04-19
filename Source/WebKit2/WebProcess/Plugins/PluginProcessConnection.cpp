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

#if ENABLE(PLUGIN_PROCESS)

#include "NPRemoteObjectMap.h"
#include "PluginProcessConnectionManager.h"
#include "PluginProxy.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/FileSystem.h>

using namespace WebCore;

namespace WebKit {

// The timeout, in seconds, when sending sync messages to the plug-in.
static const double syncMessageTimeout = 45;

static double defaultSyncMessageTimeout(const String& pluginPath)
{
    // We don't want a message timeout for the AppleConnect plug-in.
    // FIXME: We should key this off something other than the path.
    if (pathGetFileName(pluginPath) == "AppleConnect.plugin")
        return CoreIPC::Connection::NoTimeout;

    return syncMessageTimeout;
}

PluginProcessConnection::PluginProcessConnection(PluginProcessConnectionManager* pluginProcessConnectionManager, const String& pluginPath, CoreIPC::Connection::Identifier connectionIdentifier)
    : m_pluginProcessConnectionManager(pluginProcessConnectionManager)
    , m_pluginPath(pluginPath)
{
    m_connection = CoreIPC::Connection::createClientConnection(connectionIdentifier, this, WebProcess::shared().runLoop());

    m_connection->setDefaultSyncMessageTimeout(defaultSyncMessageTimeout(m_pluginPath));
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

void PluginProcessConnection::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (arguments->destinationID()) {
        if (PluginProxy* pluginProxy = m_plugins.get(arguments->destinationID()))
            pluginProxy->didReceivePluginProxyMessage(connection, messageID, arguments);
        return;
    }

    ASSERT_NOT_REACHED();
}

CoreIPC::SyncReplyMode PluginProcessConnection::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    if (messageID.is<CoreIPC::MessageClassNPObjectMessageReceiver>())
        return m_npRemoteObjectMap->didReceiveSyncMessage(connection, messageID, arguments, reply);

    if (PluginProxy* pluginProxy = m_plugins.get(arguments->destinationID()))
        return pluginProxy->didReceiveSyncPluginProxyMessage(connection, messageID, arguments, reply);

    ASSERT_NOT_REACHED();
    return CoreIPC::AutomaticReply;
}

void PluginProcessConnection::didClose(CoreIPC::Connection*)
{
    // The plug-in process must have crashed.
    for (HashMap<uint64_t, PluginProxy*>::const_iterator::Values it = m_plugins.begin().values(), end = m_plugins.end().values(); it != end; ++it) {
        PluginProxy* pluginProxy = (*it);

        pluginProxy->pluginProcessCrashed();
    }
}

void PluginProcessConnection::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID)
{
}

void PluginProcessConnection::syncMessageSendTimedOut(CoreIPC::Connection*)
{
    WebProcess::shared().connection()->send(Messages::WebProcessProxy::PluginSyncMessageSendTimedOut(m_pluginPath), 0);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
