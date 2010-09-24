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

#if ENABLE(PLUGIN_PROCESS)

#include "PluginProcessProxy.h"

#include "PluginProcessManager.h"
#include "PluginProcessMessages.h"
#include "RunLoop.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

PassOwnPtr<PluginProcessProxy> PluginProcessProxy::create(PluginProcessManager* PluginProcessManager, const PluginInfoStore::Plugin& pluginInfo)
{
    return adoptPtr(new PluginProcessProxy(PluginProcessManager, pluginInfo));
}

PluginProcessProxy::PluginProcessProxy(PluginProcessManager* PluginProcessManager, const PluginInfoStore::Plugin& pluginInfo)
    : m_pluginProcessManager(PluginProcessManager)
    , m_pluginInfo(pluginInfo)
{
    ProcessLauncher::LaunchOptions launchOptions;
    launchOptions.processType = ProcessLauncher::PluginProcess;
#if PLATFORM(MAC)
    launchOptions.architecture = pluginInfo.pluginArchitecture;
#endif

    m_processLauncher = ProcessLauncher::create(this, launchOptions);
}

void PluginProcessProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    // FIXME: Implement.
}

void PluginProcessProxy::didClose(CoreIPC::Connection*)
{
    // Tell the plug-in process manager to forget about this plug-in process proxy.
    m_pluginProcessManager->removePluginProcessProxy(this);
    delete this;
}

void PluginProcessProxy::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID)
{
}

void PluginProcessProxy::didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);
    
    m_connection = CoreIPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
    m_connection->open();
    
    // Initialize the plug-in host process.
    m_connection->send(Messages::PluginProcess::Initialize(m_pluginInfo.path), 0);
}

void PluginProcessProxy::didCreateWebProcessConnection(const CoreIPC::MachPort&)
{
    // FIXME: Implement.
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
