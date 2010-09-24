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

#include "PluginProcess.h"

#include "MachPort.h"
#include "NetscapePluginModule.h"
#include "PluginProcessProxyMessages.h"
#include "WebProcessConnection.h"

namespace WebKit {

static const double shutdownTimeout = 15.0;

PluginProcess& PluginProcess::shared()
{
    DEFINE_STATIC_LOCAL(PluginProcess, pluginProcess, ());
    return pluginProcess;
}

PluginProcess::PluginProcess()
    : m_shutdownTimer(RunLoop::main(), this, &PluginProcess::shutdownTimerFired)
{
}

PluginProcess::~PluginProcess()
{
}

void PluginProcess::initializeConnection(CoreIPC::Connection::Identifier serverIdentifier)
{
    ASSERT(!m_connection);

    m_connection = CoreIPC::Connection::createClientConnection(serverIdentifier, this, RunLoop::main());
    m_connection->open();
}

void PluginProcess::removeWebProcessConnection(WebProcessConnection* webProcessConnection)
{
    size_t vectorIndex = m_webProcessConnections.find(webProcessConnection);
    ASSERT(vectorIndex != notFound);

    m_webProcessConnections.remove(vectorIndex);

    if (m_webProcessConnections.isEmpty()) {
        // Start the shutdown timer.
        m_shutdownTimer.startOneShot(shutdownTimeout);
    }
}

void PluginProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
}

void PluginProcess::didClose(CoreIPC::Connection*)
{
    // The UI process has crashed, just go ahead and quit.
    // FIXME: If the plug-in is spinning in the main loop, we'll never get this message.
    RunLoop::current()->stop();
}

void PluginProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID)
{
}

void PluginProcess::initialize(const String& pluginPath)
{
    ASSERT(!m_pluginModule);

    m_pluginModule = NetscapePluginModule::getOrCreate(pluginPath);
}

void PluginProcess::createWebProcessConnection()
{
    // FIXME: This is platform specific!

    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    RefPtr<WebProcessConnection> connection = WebProcessConnection::create(listeningPort);
    m_webProcessConnections.append(connection.release());

    CoreIPC::MachPort clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    m_connection->send(Messages::PluginProcessProxy::DidCreateWebProcessConnection(clientPort), 0);

    // Stop the shutdown timer.
    m_shutdownTimer.stop();
}

void PluginProcess::shutdownTimerFired()
{
    RunLoop::current()->stop();
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

