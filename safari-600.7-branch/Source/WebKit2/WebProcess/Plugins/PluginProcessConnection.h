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

#ifndef PluginProcessConnection_h
#define PluginProcessConnection_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "Connection.h"
#include "Plugin.h"
#include "PluginProcess.h"
#include "PluginProcessAttributes.h"
#include <wtf/RefCounted.h>

namespace WebKit {

class NPRemoteObjectMap;
class PluginProcessConnectionManager;
class PluginProxy;
    
class PluginProcessConnection : public RefCounted<PluginProcessConnection>, IPC::Connection::Client {
public:
    static PassRefPtr<PluginProcessConnection> create(PluginProcessConnectionManager* pluginProcessConnectionManager, uint64_t pluginProcessToken, IPC::Connection::Identifier connectionIdentifier, bool supportsAsynchronousPluginInitialization)
    {
        return adoptRef(new PluginProcessConnection(pluginProcessConnectionManager, pluginProcessToken, connectionIdentifier, supportsAsynchronousPluginInitialization));
    }
    ~PluginProcessConnection();

    uint64_t pluginProcessToken() const { return m_pluginProcessToken; }

    IPC::Connection* connection() const { return m_connection.get(); }

    void addPluginProxy(PluginProxy*);
    void removePluginProxy(PluginProxy*);

    NPRemoteObjectMap* npRemoteObjectMap() const { return m_npRemoteObjectMap.get(); }

    bool supportsAsynchronousPluginInitialization() const { return m_supportsAsynchronousPluginInitialization; }
    WebCore::AudioHardwareActivityType audioHardwareActivity() const { return m_audioHardwareActivity; }
    
private:
    PluginProcessConnection(PluginProcessConnectionManager*, uint64_t pluginProcessToken, IPC::Connection::Identifier connectionIdentifier, bool supportsAsynchronousInitialization);

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;
    virtual void didClose(IPC::Connection*);
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // Message handlers.
    void didReceivePluginProcessConnectionMessage(IPC::Connection*, IPC::MessageDecoder&);
    void didReceiveSyncPluginProcessConnectionMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);
    void setException(const String&);
    void audioHardwareDidBecomeActive();
    void audioHardwareDidBecomeInactive();

    PluginProcessConnectionManager* m_pluginProcessConnectionManager;
    uint64_t m_pluginProcessToken;

    // The connection from the web process to the plug-in process.
    RefPtr<IPC::Connection> m_connection;

    // The plug-ins. We use a weak reference to the plug-in proxies because the plug-in view holds the strong reference.
    HashMap<uint64_t, PluginProxy*> m_plugins;

    RefPtr<NPRemoteObjectMap> m_npRemoteObjectMap;
    
    bool m_supportsAsynchronousPluginInitialization;
    WebCore::AudioHardwareActivityType m_audioHardwareActivity;
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // PluginProcessConnection_h
