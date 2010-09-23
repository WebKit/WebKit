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

#ifndef PluginProcessProxy_h
#define PluginProcessProxy_h

#if ENABLE(PLUGIN_PROCESS)

#include "Connection.h"
#include "PluginInfoStore.h"
#include "ProcessLauncher.h"
#include <wtf/Deque.h>

namespace WebKit {

class PluginProcessManager;
class WebProcessProxy;
    
class PluginProcessProxy : CoreIPC::Connection::Client, ProcessLauncher::Client {
public:
    static PassOwnPtr<PluginProcessProxy> create(PluginProcessManager*, const PluginInfoStore::Plugin&);

    const PluginInfoStore::Plugin& pluginInfo() const { return m_pluginInfo; }

private:
    PluginProcessProxy(PluginProcessManager*, const PluginInfoStore::Plugin&);

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void didClose(CoreIPC::Connection*);
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID);

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier);
    
    // The plug-in host process manager.
    PluginProcessManager* m_pluginProcessManager;
    
    // Information about the plug-in.
    PluginInfoStore::Plugin m_pluginInfo;

    // The connection to the plug-in host process.
    RefPtr<CoreIPC::Connection> m_connection;

    // The process launcher for the plug-in host process.
    RefPtr<ProcessLauncher> m_processLauncher;
};

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

#endif // PluginProcessProxy_h
