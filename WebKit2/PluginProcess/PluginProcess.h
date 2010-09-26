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

#ifndef PluginProcess_h
#define PluginProcess_h

#if ENABLE(PLUGIN_PROCESS)

#include "Connection.h"
#include "RunLoop.h"
#include <wtf/Forward.h>

namespace WebKit {

class NetscapePluginModule;
class WebProcessConnection;
        
class PluginProcess : Noncopyable, CoreIPC::Connection::Client {
public:
    static PluginProcess& shared();

    void initializeConnection(CoreIPC::Connection::Identifier);
    void removeWebProcessConnection(WebProcessConnection* webProcessConnection);

    NetscapePluginModule* netscapePluginModule() const { return m_pluginModule.get(); }

private:
    PluginProcess();
    ~PluginProcess();

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void didClose(CoreIPC::Connection*);
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID);

    // Message handlers.
    void didReceivePluginProcessMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void initialize(const String& pluginPath);
    void createWebProcessConnection();
    
    void shutdownTimerFired();

    // The connection to the UI process.
    RefPtr<CoreIPC::Connection> m_connection;

    // Our web process connections.
    Vector<RefPtr<WebProcessConnection> > m_webProcessConnections;

    // The plug-in module.
    RefPtr<NetscapePluginModule> m_pluginModule;
    
    // A timer used for the shutdown timeout.
    RunLoop::Timer<PluginProcess> m_shutdownTimer;
};

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

#endif // PluginProcess_h
