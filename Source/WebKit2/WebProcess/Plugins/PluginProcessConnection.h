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

#if ENABLE(PLUGIN_PROCESS)

#include "Connection.h"
#include "Plugin.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

// A CoreIPC connection to a plug-in process.

namespace WebKit {

class NPRemoteObjectMap;
class PluginProcessConnectionManager;
class PluginProxy;
    
class PluginProcessConnection : public RefCounted<PluginProcessConnection>, CoreIPC::Connection::Client {
public:
    static PassRefPtr<PluginProcessConnection> create(PluginProcessConnectionManager* pluginProcessConnectionManager, const String& pluginPath, CoreIPC::Connection::Identifier connectionIdentifier)
    {
        return adoptRef(new PluginProcessConnection(pluginProcessConnectionManager, pluginPath, connectionIdentifier));
    }
    ~PluginProcessConnection();

    const String& pluginPath() const { return m_pluginPath; }

    CoreIPC::Connection* connection() const { return m_connection.get(); }

    void addPluginProxy(PluginProxy*);
    void removePluginProxy(PluginProxy*);

    NPRemoteObjectMap* npRemoteObjectMap() const { return m_npRemoteObjectMap.get(); }

private:
    PluginProcessConnection(PluginProcessConnectionManager* pluginProcessConnectionManager, const String& pluginPath, CoreIPC::Connection::Identifier connectionIdentifier);

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);
    virtual void didClose(CoreIPC::Connection*);
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID);

    // Message handlers.
    void didReceiveSyncPluginProcessConnectionMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);
    void setException(const String&);

    PluginProcessConnectionManager* m_pluginProcessConnectionManager;
    String m_pluginPath;

    // The connection from the web process to the plug-in process.
    RefPtr<CoreIPC::Connection> m_connection;

    // The plug-ins. We use a weak reference to the plug-in proxies because the plug-in view holds the strong reference.
    HashMap<uint64_t, PluginProxy*> m_plugins;

    RefPtr<NPRemoteObjectMap> m_npRemoteObjectMap;
};

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

#endif // PluginProcessConnection_h
