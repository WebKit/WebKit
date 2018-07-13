/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "StorageProcessProxy.h"

#include "NetworkProcessMessages.h"
#include "ServiceWorkerProcessProxy.h"
#include "StorageProcessMessages.h"
#include "StorageProcessProxyMessages.h"
#include "WebProcessPool.h"
#include "WebsiteData.h"
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generateCallbackID()
{
    static uint64_t callbackID;

    return ++callbackID;
}

Ref<StorageProcessProxy> StorageProcessProxy::create(WebProcessPool& processPool)
{
    return adoptRef(*new StorageProcessProxy(processPool));
}

StorageProcessProxy::StorageProcessProxy(WebProcessPool& processPool)
    : ChildProcessProxy(processPool.alwaysRunsAtBackgroundPriority())
    , m_processPool(processPool)
    , m_numPendingConnectionRequests(0)
{
    connect();
}

StorageProcessProxy::~StorageProcessProxy()
{
    ASSERT(m_pendingFetchWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataForOriginsCallbacks.isEmpty());
}

void StorageProcessProxy::terminateForTesting()
{
    for (auto& callback : m_pendingFetchWebsiteDataCallbacks.values())
        callback({ });

    for (auto& callback : m_pendingDeleteWebsiteDataCallbacks.values())
        callback();

    for (auto& callback : m_pendingDeleteWebsiteDataForOriginsCallbacks.values())
        callback();
    
    m_pendingFetchWebsiteDataCallbacks.clear();
    m_pendingDeleteWebsiteDataCallbacks.clear();
    m_pendingDeleteWebsiteDataForOriginsCallbacks.clear();
    
    terminate();
}

void StorageProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Storage;
    ChildProcessProxy::getLaunchOptions(launchOptions);
}

void StorageProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

void StorageProcessProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::StorageProcessProxy::messageReceiverName()) {
        didReceiveStorageProcessProxyMessage(connection, decoder);
        return;
    }
}

void StorageProcessProxy::fetchWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, WTF::Function<void (WebsiteData)>&& completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    m_pendingFetchWebsiteDataCallbacks.add(callbackID, WTFMove(completionHandler));

    send(Messages::StorageProcess::FetchWebsiteData(sessionID, dataTypes, callbackID), 0);
}

void StorageProcessProxy::deleteWebsiteData(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, WTF::Function<void ()>&& completionHandler)
{
    auto callbackID = generateCallbackID();

    m_pendingDeleteWebsiteDataCallbacks.add(callbackID, WTFMove(completionHandler));
    send(Messages::StorageProcess::DeleteWebsiteData(sessionID, dataTypes, modifiedSince, callbackID), 0);
}

void StorageProcessProxy::deleteWebsiteDataForOrigins(PAL::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, const Vector<WebCore::SecurityOriginData>& origins, WTF::Function<void()>&& completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    m_pendingDeleteWebsiteDataForOriginsCallbacks.add(callbackID, WTFMove(completionHandler));

    send(Messages::StorageProcess::DeleteWebsiteDataForOrigins(sessionID, dataTypes, origins, callbackID), 0);
}

void StorageProcessProxy::getStorageProcessConnection(WebProcessProxy& webProcessProxy, Messages::WebProcessProxy::GetStorageProcessConnection::DelayedReply&& reply)
{
    m_pendingConnectionReplies.append(WTFMove(reply));

    if (state() == State::Launching) {
        m_numPendingConnectionRequests++;
        return;
    }

    bool isServiceWorkerProcess = false;
    SecurityOriginData securityOrigin;
#if ENABLE(SERVICE_WORKER)
    if (is<ServiceWorkerProcessProxy>(webProcessProxy)) {
        isServiceWorkerProcess = true;
        securityOrigin = downcast<ServiceWorkerProcessProxy>(webProcessProxy).securityOrigin();
    }
#endif

    send(Messages::StorageProcess::CreateStorageToWebProcessConnection(isServiceWorkerProcess, securityOrigin), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void StorageProcessProxy::didClose(IPC::Connection&)
{
    auto protectedProcessPool = makeRef(m_processPool);

    // The storage process must have crashed or exited, so send any pending sync replies we might have.
    while (!m_pendingConnectionReplies.isEmpty()) {
        auto reply = m_pendingConnectionReplies.takeFirst();

#if USE(UNIX_DOMAIN_SOCKETS)
        reply(IPC::Attachment());
#elif OS(DARWIN)
        reply(IPC::Attachment(0, MACH_MSG_TYPE_MOVE_SEND));
#elif OS(WINDOWS)
        reply(IPC::Attachment());
#else
        notImplemented();
#endif
    }

    while (!m_pendingFetchWebsiteDataCallbacks.isEmpty())
        m_pendingFetchWebsiteDataCallbacks.take(m_pendingFetchWebsiteDataCallbacks.begin()->key)(WebsiteData { });

    while (!m_pendingDeleteWebsiteDataCallbacks.isEmpty())
        m_pendingDeleteWebsiteDataCallbacks.take(m_pendingDeleteWebsiteDataCallbacks.begin()->key)();

    while (!m_pendingDeleteWebsiteDataForOriginsCallbacks.isEmpty())
        m_pendingDeleteWebsiteDataForOriginsCallbacks.take(m_pendingDeleteWebsiteDataForOriginsCallbacks.begin()->key)();

    // Tell ProcessPool to forget about this storage process. This may cause us to be deleted.
    m_processPool.storageProcessCrashed(this);
}

void StorageProcessProxy::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
}

void StorageProcessProxy::didCreateStorageToWebProcessConnection(const IPC::Attachment& connectionIdentifier)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    auto reply = m_pendingConnectionReplies.takeFirst();

#if USE(UNIX_DOMAIN_SOCKETS)
    reply(connectionIdentifier);
#elif OS(DARWIN)
    reply(IPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND));
#elif OS(WINDOWS)
    reply(connectionIdentifier.handle());
#else
    notImplemented();
#endif
}

void StorageProcessProxy::didFetchWebsiteData(uint64_t callbackID, const WebsiteData& websiteData)
{
    auto callback = m_pendingFetchWebsiteDataCallbacks.take(callbackID);
    callback(websiteData);
}

void StorageProcessProxy::didDeleteWebsiteData(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataCallbacks.take(callbackID);
    callback();
}

void StorageProcessProxy::didDeleteWebsiteDataForOrigins(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataForOriginsCallbacks.take(callbackID);
    callback();
}

#if ENABLE(SANDBOX_EXTENSIONS)
void StorageProcessProxy::getSandboxExtensionsForBlobFiles(uint64_t requestID, const Vector<String>& paths)
{
    SandboxExtension::HandleArray extensions;
    extensions.allocate(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        // ReadWrite is required for creating hard links, which is something that might be done with these extensions.
        SandboxExtension::createHandle(paths[i], SandboxExtension::Type::ReadWrite, extensions[i]);
    }

    send(Messages::StorageProcess::DidGetSandboxExtensionsForBlobFiles(requestID, extensions), 0);
}
#endif

void StorageProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        send(Messages::StorageProcess::CreateStorageToWebProcessConnection(false, { }), 0);
    
    m_numPendingConnectionRequests = 0;
}

#if ENABLE(SERVICE_WORKER)
void StorageProcessProxy::establishWorkerContextConnectionToStorageProcess(SecurityOriginData&& origin)
{
    m_processPool.establishWorkerContextConnectionToStorageProcess(*this, WTFMove(origin), std::nullopt);
}

void StorageProcessProxy::establishWorkerContextConnectionToStorageProcessForExplicitSession(SecurityOriginData&& origin, PAL::SessionID sessionID)
{
    m_processPool.establishWorkerContextConnectionToStorageProcess(*this, WTFMove(origin), sessionID);
}
#endif

} // namespace WebKit
