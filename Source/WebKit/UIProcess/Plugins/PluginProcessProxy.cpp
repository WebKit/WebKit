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
#include "PluginProcessProxy.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "ChildProcessMessages.h"
#include "PluginProcessConnectionManagerMessages.h"
#include "PluginProcessCreationParameters.h"
#include "PluginProcessManager.h"
#include "PluginProcessMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/NotImplemented.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

static const Seconds minimumLifetime { 2_min };
static const Seconds snapshottingMinimumLifetime { 30_s };

static const Seconds shutdownTimeout { 1_min };
static const Seconds snapshottingShutdownTimeout { 15_s };

static uint64_t generatePluginProcessCallbackID()
{
    static uint64_t callbackID;

    return ++callbackID;
}

Ref<PluginProcessProxy> PluginProcessProxy::create(PluginProcessManager* PluginProcessManager, const PluginProcessAttributes& pluginProcessAttributes, uint64_t pluginProcessToken)
{
    return adoptRef(*new PluginProcessProxy(PluginProcessManager, pluginProcessAttributes, pluginProcessToken));
}

PluginProcessProxy::PluginProcessProxy(PluginProcessManager* PluginProcessManager, const PluginProcessAttributes& pluginProcessAttributes, uint64_t pluginProcessToken)
    : m_pluginProcessManager(PluginProcessManager)
    , m_pluginProcessAttributes(pluginProcessAttributes)
    , m_pluginProcessToken(pluginProcessToken)
    , m_numPendingConnectionRequests(0)
#if PLATFORM(COCOA)
    , m_modalWindowIsShowing(false)
    , m_fullscreenWindowIsShowing(false)
    , m_preFullscreenAppPresentationOptions(0)
#endif
{
    connect();
}

PluginProcessProxy::~PluginProcessProxy()
{
    if (m_connection)
        m_connection->invalidate();

    ASSERT(m_pendingFetchWebsiteDataRequests.isEmpty());
    ASSERT(m_pendingFetchWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataRequests.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataCallbacks.isEmpty());
}

void PluginProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    platformGetLaunchOptionsWithAttributes(launchOptions, m_pluginProcessAttributes);
    ChildProcessProxy::getLaunchOptions(launchOptions);
}

void PluginProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

// Asks the plug-in process to create a new connection to a web process. The connection identifier will be 
// encoded in the given argument encoder and sent back to the connection of the given web process.
void PluginProcessProxy::getPluginProcessConnection(Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply&& reply)
{
    m_pendingConnectionReplies.append(WTFMove(reply));

    if (state() == State::Launching) {
        m_numPendingConnectionRequests++;
        return;
    }
    
    // Ask the plug-in process to create a connection. Since the plug-in can be waiting for a synchronous reply
    // we need to make sure that this message is always processed, even when the plug-in is waiting for a synchronus reply.
    m_connection->send(Messages::PluginProcess::CreateWebProcessConnection(), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void PluginProcessProxy::fetchWebsiteData(CompletionHandler<void (Vector<String>)>&& completionHandler)
{
    uint64_t callbackID = generatePluginProcessCallbackID();
    m_pendingFetchWebsiteDataCallbacks.set(callbackID, WTFMove(completionHandler));

    if (state() == State::Launching) {
        m_pendingFetchWebsiteDataRequests.append(callbackID);
        return;
    }

    m_connection->send(Messages::PluginProcess::GetSitesWithData(callbackID), 0);
}

void PluginProcessProxy::deleteWebsiteData(WallTime modifiedSince, CompletionHandler<void ()>&& completionHandler)
{
    uint64_t callbackID = generatePluginProcessCallbackID();
    m_pendingDeleteWebsiteDataCallbacks.set(callbackID, WTFMove(completionHandler));

    if (state() == State::Launching) {
        m_pendingDeleteWebsiteDataRequests.append({ modifiedSince, callbackID });
        return;
    }

    m_connection->send(Messages::PluginProcess::DeleteWebsiteData(modifiedSince, callbackID), 0);
}

void PluginProcessProxy::deleteWebsiteDataForHostNames(const Vector<String>& hostNames, CompletionHandler<void ()>&& completionHandler)
{
    uint64_t callbackID = generatePluginProcessCallbackID();
    m_pendingDeleteWebsiteDataForHostNamesCallbacks.set(callbackID, WTFMove(completionHandler));

    if (state() == State::Launching) {
        m_pendingDeleteWebsiteDataForHostNamesRequests.append({ hostNames, callbackID });
        return;
    }

    m_connection->send(Messages::PluginProcess::DeleteWebsiteDataForHostNames(hostNames, callbackID), 0);
}

#if OS(LINUX)
void PluginProcessProxy::sendMemoryPressureEvent(bool isCritical)
{
    if (state() == State::Launching)
        return;

    m_connection->send(Messages::ChildProcess::DidReceiveMemoryPressureEvent(isCritical), 0);
}
#endif

void PluginProcessProxy::pluginProcessCrashedOrFailedToLaunch()
{
    // The plug-in process must have crashed or exited, send any pending sync replies we might have.
    while (!m_pendingConnectionReplies.isEmpty()) {
        auto reply = m_pendingConnectionReplies.takeFirst();

#if USE(UNIX_DOMAIN_SOCKETS)
        reply(IPC::Attachment(), false);
#elif OS(DARWIN)
        reply(IPC::Attachment(0, MACH_MSG_TYPE_MOVE_SEND), false);
#else
        notImplemented();
#endif
    }

    m_pendingFetchWebsiteDataRequests.clear();
    for (auto&& callback : m_pendingFetchWebsiteDataCallbacks.values())
        callback({ });
    m_pendingFetchWebsiteDataCallbacks.clear();

    m_pendingDeleteWebsiteDataRequests.clear();
    for (auto&& callback : m_pendingDeleteWebsiteDataCallbacks.values())
        callback();
    m_pendingDeleteWebsiteDataRequests.clear();

    m_pendingDeleteWebsiteDataForHostNamesRequests.clear();
    for (auto&& callback : m_pendingDeleteWebsiteDataForHostNamesCallbacks.values())
        callback();
    m_pendingDeleteWebsiteDataForHostNamesCallbacks.clear();

    // Tell the plug-in process manager to forget about this plug-in process proxy. This may cause us to be deleted.
    m_pluginProcessManager->removePluginProcessProxy(this);
}

void PluginProcessProxy::didClose(IPC::Connection&)
{
#if PLATFORM(COCOA)
    if (m_modalWindowIsShowing)
        endModal();

    if (m_fullscreenWindowIsShowing)
        exitFullscreen();
#endif

    const Vector<WebProcessPool*>& processPools = WebProcessPool::allProcessPools();
    for (size_t i = 0; i < processPools.size(); ++i)
        processPools[i]->sendToAllProcesses(Messages::PluginProcessConnectionManager::PluginProcessCrashed(m_pluginProcessToken));

    // This will cause us to be deleted.
    pluginProcessCrashedOrFailedToLaunch();
}

void PluginProcessProxy::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
}

void PluginProcessProxy::didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        pluginProcessCrashedOrFailedToLaunch();
        return;
    }

    m_connection = IPC::Connection::createServerConnection(connectionIdentifier, *this);

    m_connection->open();
    
    PluginProcessCreationParameters parameters;
    parameters.processType = m_pluginProcessAttributes.processType;
    if (parameters.processType == PluginProcessTypeSnapshot) {
        parameters.minimumLifetime = snapshottingMinimumLifetime;
        parameters.terminationTimeout = snapshottingShutdownTimeout;
    } else {
        parameters.minimumLifetime = minimumLifetime;
        parameters.terminationTimeout = shutdownTimeout;
    }

    platformInitializePluginProcess(parameters);

    // Initialize the plug-in host process.
    m_connection->send(Messages::PluginProcess::InitializePluginProcess(parameters), 0);

#if PLATFORM(COCOA)
    m_connection->send(Messages::PluginProcess::SetQOS(pluginProcessLatencyQOS(), pluginProcessThroughputQOS()), 0);
#endif

    for (auto callbackID : m_pendingFetchWebsiteDataRequests)
        m_connection->send(Messages::PluginProcess::GetSitesWithData(callbackID), 0);
    m_pendingFetchWebsiteDataRequests.clear();

    for (auto& request : m_pendingDeleteWebsiteDataRequests)
        m_connection->send(Messages::PluginProcess::DeleteWebsiteData(request.modifiedSince, request.callbackID), 0);
    m_pendingDeleteWebsiteDataRequests.clear();

    for (auto& request : m_pendingDeleteWebsiteDataForHostNamesRequests)
        m_connection->send(Messages::PluginProcess::DeleteWebsiteDataForHostNames(request.hostNames, request.callbackID), 0);
    m_pendingDeleteWebsiteDataForHostNamesRequests.clear();

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        m_connection->send(Messages::PluginProcess::CreateWebProcessConnection(), 0);
    
    m_numPendingConnectionRequests = 0;

#if PLATFORM(COCOA)
    if (!PluginProcessManager::singleton().processSuppressionDisabled())
        setProcessSuppressionEnabled(true);
#endif
}

void PluginProcessProxy::didCreateWebProcessConnection(const IPC::Attachment& connectionIdentifier, bool supportsAsynchronousPluginInitialization)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    // Grab the first pending connection reply.
    auto reply = m_pendingConnectionReplies.takeFirst();

#if USE(UNIX_DOMAIN_SOCKETS)
    reply(connectionIdentifier, supportsAsynchronousPluginInitialization);
#elif OS(DARWIN)
    reply(IPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND), supportsAsynchronousPluginInitialization);
#else
    notImplemented();
#endif
}

void PluginProcessProxy::didGetSitesWithData(const Vector<String>& sites, uint64_t callbackID)
{
    auto callback = m_pendingFetchWebsiteDataCallbacks.take(callbackID);
    callback(sites);
}

void PluginProcessProxy::didDeleteWebsiteData(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataCallbacks.take(callbackID);
    callback();
}

void PluginProcessProxy::didDeleteWebsiteDataForHostNames(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataForHostNamesCallbacks.take(callbackID);
    callback();
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
