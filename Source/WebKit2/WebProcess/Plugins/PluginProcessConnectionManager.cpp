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
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <wtf/StdLibExtras.h>

#if PLATFORM(MAC)
#include "MachPort.h"
#endif

namespace WebKit {

PluginProcessConnectionManager& PluginProcessConnectionManager::shared()
{
    DEFINE_STATIC_LOCAL(PluginProcessConnectionManager, pluginProcessConnectionManager, ());
    return pluginProcessConnectionManager;
}

PluginProcessConnectionManager::PluginProcessConnectionManager()
{
}

PluginProcessConnectionManager::~PluginProcessConnectionManager()
{
}

PluginProcessConnection* PluginProcessConnectionManager::getPluginProcessConnection(const String& pluginPath)
{
    for (size_t i = 0; i < m_pluginProcessConnections.size(); ++i) {
        if (m_pluginProcessConnections[i]->pluginPath() == pluginPath)
            return m_pluginProcessConnections[i].get();
    }

    CoreIPC::Connection::Identifier connectionIdentifier;
#if PLATFORM(MAC)
    CoreIPC::MachPort connectionMachPort;

    if (!WebProcess::shared().connection()->sendSync(Messages::WebProcessProxy::GetPluginProcessConnection(pluginPath), Messages::WebProcessProxy::GetPluginProcessConnection::Reply(connectionMachPort), 0))
        return 0;

    connectionIdentifier = connectionMachPort.port();
#else
    // FIXME: Implement.
    connectionIdentifier = 0;
    ASSERT_NOT_REACHED();
#endif
    if (!connectionIdentifier)
        return 0;

    RefPtr<PluginProcessConnection> pluginProcessConnection = PluginProcessConnection::create(this, pluginPath, connectionIdentifier);
    m_pluginProcessConnections.append(pluginProcessConnection);

    return pluginProcessConnection.get();
}

void PluginProcessConnectionManager::removePluginProcessConnection(PluginProcessConnection* pluginProcessConnection)
{
    size_t vectorIndex = m_pluginProcessConnections.find(pluginProcessConnection);
    ASSERT(vectorIndex != notFound);

    m_pluginProcessConnections.remove(vectorIndex);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
