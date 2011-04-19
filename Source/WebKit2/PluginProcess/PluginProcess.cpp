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
#include "PluginProcess.h"

#if ENABLE(PLUGIN_PROCESS)

#include "ArgumentCoders.h"
#include "NetscapePluginModule.h"
#include "PluginProcessProxyMessages.h"
#include "PluginProcessCreationParameters.h"
#include "WebProcessConnection.h"

#if PLATFORM(MAC)
#include "MachPort.h"
#endif

namespace WebKit {

static const double shutdownTimeout = 15.0;

PluginProcess& PluginProcess::shared()
{
    DEFINE_STATIC_LOCAL(PluginProcess, pluginProcess, ());
    return pluginProcess;
}

PluginProcess::PluginProcess()
    : ChildProcess(shutdownTimeout)
#if PLATFORM(MAC)
    , m_compositingRenderServerPort(MACH_PORT_NULL)
#endif
{
}

PluginProcess::~PluginProcess()
{
}

void PluginProcess::initialize(CoreIPC::Connection::Identifier serverIdentifier, RunLoop* runLoop)
{
    ASSERT(!m_connection);

    m_connection = CoreIPC::Connection::createClientConnection(serverIdentifier, this, runLoop);
    m_connection->setDidCloseOnConnectionWorkQueueCallback(didCloseOnConnectionWorkQueue);
    m_connection->open();
}

void PluginProcess::removeWebProcessConnection(WebProcessConnection* webProcessConnection)
{
    size_t vectorIndex = m_webProcessConnections.find(webProcessConnection);
    ASSERT(vectorIndex != notFound);

    m_webProcessConnections.remove(vectorIndex);
    
    if (m_webProcessConnections.isEmpty() && m_pluginModule) {
        // Decrement the load count. This is balanced by a call to incrementLoadCount in createWebProcessConnection.
        m_pluginModule->decrementLoadCount();
    }        

    enableTermination();
}

NetscapePluginModule* PluginProcess::netscapePluginModule()
{
    if (!m_pluginModule) {
        ASSERT(!m_pluginPath.isNull());
        m_pluginModule = NetscapePluginModule::getOrCreate(m_pluginPath);

#if PLATFORM(MAC)
        if (m_pluginModule) {
            if (m_pluginModule->pluginQuirks().contains(PluginQuirks::PrognameShouldBeWebKitPluginHost))
                setprogname("WebKitPluginHost");
        }
#endif
    }

    return m_pluginModule.get();
}

bool PluginProcess::shouldTerminate()
{
    ASSERT(m_webProcessConnections.isEmpty());

    return true;
}

void PluginProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceivePluginProcessMessage(connection, messageID, arguments);
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

void PluginProcess::syncMessageSendTimedOut(CoreIPC::Connection*)
{
}

void PluginProcess::initializePluginProcess(const PluginProcessCreationParameters& parameters)
{
    ASSERT(!m_pluginModule);

    m_pluginPath = parameters.pluginPath;

    platformInitialize(parameters);
}

void PluginProcess::createWebProcessConnection()
{
    bool didHaveAnyWebProcessConnections = !m_webProcessConnections.isEmpty();

#if PLATFORM(MAC)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    RefPtr<WebProcessConnection> connection = WebProcessConnection::create(listeningPort);
    m_webProcessConnections.append(connection.release());

    CoreIPC::MachPort clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    m_connection->send(Messages::PluginProcessProxy::DidCreateWebProcessConnection(clientPort), 0);
#else
    // FIXME: Implement.
    ASSERT_NOT_REACHED();
#endif

    if (NetscapePluginModule* module = netscapePluginModule()) {
        if (!didHaveAnyWebProcessConnections) {
            // Increment the load count. This is matched by a call to decrementLoadCount in removeWebProcessConnection.
            // We do this so that the plug-in module's NP_Shutdown won't be called until right before exiting.
            module->incrementLoadCount();
        }
    }

    disableTermination();
}

void PluginProcess::getSitesWithData(uint64_t callbackID)
{
    LocalTerminationDisabler terminationDisabler(*this);

    Vector<String> sites;
    if (NetscapePluginModule* module = netscapePluginModule())
        sites = module->sitesWithData();

    m_connection->send(Messages::PluginProcessProxy::DidGetSitesWithData(sites, callbackID), 0);
}

void PluginProcess::clearSiteData(const Vector<String>& sites, uint64_t flags, uint64_t maxAgeInSeconds, uint64_t callbackID)
{
    LocalTerminationDisabler terminationDisabler(*this);

    if (NetscapePluginModule* module = netscapePluginModule()) {
        if (sites.isEmpty()) {
            // Clear everything.
            module->clearSiteData(String(), flags, maxAgeInSeconds);
        } else {
            for (size_t i = 0; i < sites.size(); ++i)
                module->clearSiteData(sites[i], flags, maxAgeInSeconds);
        }
    }

    m_connection->send(Messages::PluginProcessProxy::DidClearSiteData(callbackID), 0);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

