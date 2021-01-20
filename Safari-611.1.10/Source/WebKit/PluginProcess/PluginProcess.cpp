/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "ArgumentCoders.h"
#include "Attachment.h"
#include "AuxiliaryProcessMessages.h"
#include "NetscapePlugin.h"
#include "NetscapePluginModule.h"
#include "PluginProcessConnectionMessages.h"
#include "PluginProcessCreationParameters.h"
#include "PluginProcessProxyMessages.h"
#include "WebProcessConnection.h"
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotImplemented.h>
#include <unistd.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>

#if PLATFORM(MAC)
#include <crt_externs.h>
#endif

namespace WebKit {

using namespace WebCore;

NO_RETURN static void callExit(IPC::Connection*)
{
    _exit(EXIT_SUCCESS);
}

PluginProcess& PluginProcess::singleton()
{
    static NeverDestroyed<PluginProcess> pluginProcess;
    return pluginProcess;
}

PluginProcess::PluginProcess()
    : m_supportsAsynchronousPluginInitialization(false)
    , m_minimumLifetimeTimer(RunLoop::main(), this, &PluginProcess::minimumLifetimeTimerFired)
    , m_connectionActivity("PluginProcess connection activity.")
{
    NetscapePlugin::setSetExceptionFunction(WebProcessConnection::setGlobalException);
}

PluginProcess::~PluginProcess()
{
}

void PluginProcess::initializeProcess(const AuxiliaryProcessInitializationParameters& parameters)
{
    WTF::setProcessPrivileges(allPrivileges());
    WebCore::NetworkStorageSession::permitProcessToUseCookieAPI(true);
    m_pluginPath = parameters.extraInitializationData.get("plugin-path");
    platformInitializeProcess(parameters);
}

void PluginProcess::initializeConnection(IPC::Connection* connection)
{
    AuxiliaryProcess::initializeConnection(connection);

    // We call _exit() directly from the background queue in case the main thread is unresponsive
    // and AuxiliaryProcess::didClose() does not get called.
    connection->setDidCloseOnConnectionWorkQueueCallback(callExit);
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
                *const_cast<const char**>(_NSGetProgname()) = "WebKitPluginHost";
        }
#endif
    }

    return m_pluginModule.get();
}

bool PluginProcess::shouldTerminate()
{
    return m_webProcessConnections.isEmpty();
}

void PluginProcess::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
#if OS(LINUX)
    if (decoder.messageReceiverName() == Messages::AuxiliaryProcess::messageReceiverName()) {
        AuxiliaryProcess::didReceiveMessage(connection, decoder);
        return;
    }
#endif

    didReceivePluginProcessMessage(connection, decoder);
}

void PluginProcess::initializePluginProcess(PluginProcessCreationParameters&& parameters)
{
    ASSERT(!m_pluginModule);

    auto& memoryPressureHandler = MemoryPressureHandler::singleton();
    memoryPressureHandler.setLowMemoryHandler([this] (Critical, Synchronous) {
        if (shouldTerminate())
            terminate();
    });
    memoryPressureHandler.install();

    m_supportsAsynchronousPluginInitialization = parameters.supportsAsynchronousPluginInitialization;
    setMinimumLifetime(parameters.minimumLifetime);
    setTerminationTimeout(parameters.terminationTimeout);

    platformInitializePluginProcess(WTFMove(parameters));
}

void PluginProcess::createWebProcessConnection()
{
    // FIXME: Merge this with AuxiliaryProcess::createIPCConnectionPair().

    bool didHaveAnyWebProcessConnections = !m_webProcessConnections.isEmpty();

#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();

    auto connection = WebProcessConnection::create(socketPair.server);
    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientSocket(socketPair.client);
    parentProcessConnection()->send(Messages::PluginProcessProxy::DidCreateWebProcessConnection(clientSocket, m_supportsAsynchronousPluginInitialization), 0);
#elif OS(DARWIN)
    // Create the listening port.
    mach_port_t listeningPort = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    if (kr != KERN_SUCCESS) {
        LOG_ERROR("Could not allocate mach port, error %x", kr);
        CRASH();
    }

    // Create a listening connection.
    auto connection = WebProcessConnection::create(IPC::Connection::Identifier(listeningPort));

    m_webProcessConnections.append(WTFMove(connection));

    IPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::PluginProcessProxy::DidCreateWebProcessConnection(clientPort, m_supportsAsynchronousPluginInitialization), 0);
#else
    notImplemented();
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
    Vector<String> sites;
    if (NetscapePluginModule* module = netscapePluginModule())
        sites = module->sitesWithData();

    parentProcessConnection()->send(Messages::PluginProcessProxy::DidGetSitesWithData(sites, callbackID), 0);
}

void PluginProcess::deleteWebsiteData(WallTime modifiedSince, uint64_t callbackID)
{
    if (auto* module = netscapePluginModule()) {
        auto currentTime = WallTime::now();

        if (currentTime > modifiedSince) {
            uint64_t maximumAge = (currentTime - modifiedSince).secondsAs<uint64_t>();

            module->clearSiteData(String(), NP_CLEAR_ALL, maximumAge);
        }
    }

    parentProcessConnection()->send(Messages::PluginProcessProxy::DidDeleteWebsiteData(callbackID), 0);
}

void PluginProcess::deleteWebsiteDataForHostNames(const Vector<String>& hostNames, uint64_t callbackID)
{
    if (auto* module = netscapePluginModule()) {
        for (auto& hostName : hostNames)
            module->clearSiteData(hostName, NP_CLEAR_ALL, std::numeric_limits<uint64_t>::max());
    }

    parentProcessConnection()->send(Messages::PluginProcessProxy::DidDeleteWebsiteDataForHostNames(callbackID), 0);
}

void PluginProcess::setMinimumLifetime(Seconds lifetime)
{
    if (lifetime <= 0_s)
        return;
    
    disableTermination();
    
    m_minimumLifetimeTimer.startOneShot(lifetime);
}

void PluginProcess::minimumLifetimeTimerFired()
{
    enableTermination();
}

#if !PLATFORM(COCOA)
void PluginProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters&)
{
}

void PluginProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

