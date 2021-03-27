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

#pragma once

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "Connection.h"
#include "Plugin.h"
#include "WebProcessConnectionMessagesReplies.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class NPRemoteObjectMap;
class PluginControllerProxy;
struct PluginCreationParameters;
    
// A connection from a plug-in process to a web process.

class WebProcessConnection : public RefCounted<WebProcessConnection>, IPC::Connection::Client {
public:
    static Ref<WebProcessConnection> create(IPC::Connection::Identifier);
    virtual ~WebProcessConnection();

    IPC::Connection* connection() const { return m_connection.get(); }
    NPRemoteObjectMap* npRemoteObjectMap() const { return m_npRemoteObjectMap.get(); }

    void removePluginControllerProxy(PluginControllerProxy*, Plugin*);

    static void setGlobalException(const String&);
    
private:
    WebProcessConnection(IPC::Connection::Identifier);

    void addPluginControllerProxy(std::unique_ptr<PluginControllerProxy>);

    void destroyPluginControllerProxy(PluginControllerProxy*);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    // Message handlers.
    void didReceiveWebProcessConnectionMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncWebProcessConnectionMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);
    void createPlugin(const PluginCreationParameters&, Messages::WebProcessConnection::CreatePluginDelayedReply&&);
    void createPluginAsynchronously(const PluginCreationParameters&);
    void destroyPlugin(uint64_t pluginInstanceID, bool asynchronousCreationIncomplete, Messages::WebProcessConnection::DestroyPluginDelayedReply&&);
    
    void createPluginInternal(const PluginCreationParameters&, bool& result, bool& wantsWheelEvents, uint32_t& remoteLayerClientID);

    RefPtr<IPC::Connection> m_connection;

    HashMap<uint64_t, std::unique_ptr<PluginControllerProxy>> m_pluginControllers;
    RefPtr<NPRemoteObjectMap> m_npRemoteObjectMap;
    HashSet<uint64_t> m_asynchronousInstanceIDsToIgnore;
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
