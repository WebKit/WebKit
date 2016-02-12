/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "DatabaseProcessProxy.h"

#include "DatabaseProcessMessages.h"
#include "DatabaseProcessProxyMessages.h"
#include "WebProcessPool.h"
#include "WebsiteData.h"
#include <WebCore/NotImplemented.h>

#if ENABLE(DATABASE_PROCESS)

using namespace WebCore;

namespace WebKit {

static uint64_t generateCallbackID()
{
    static uint64_t callbackID;

    return ++callbackID;
}

Ref<DatabaseProcessProxy> DatabaseProcessProxy::create(WebProcessPool* processPool)
{
    return adoptRef(*new DatabaseProcessProxy(processPool));
}

DatabaseProcessProxy::DatabaseProcessProxy(WebProcessPool* processPool)
    : m_processPool(processPool)
    , m_numPendingConnectionRequests(0)
{
    connect();
}

DatabaseProcessProxy::~DatabaseProcessProxy()
{
    ASSERT(m_pendingFetchWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataForOriginsCallbacks.isEmpty());
}

void DatabaseProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Database;
    ChildProcessProxy::getLaunchOptions(launchOptions);
}

void DatabaseProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

void DatabaseProcessProxy::didReceiveMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::DatabaseProcessProxy::messageReceiverName()) {
        didReceiveDatabaseProcessProxyMessage(connection, decoder);
        return;
    }
}

void DatabaseProcessProxy::fetchWebsiteData(SessionID sessionID, WebsiteDataTypes dataTypes, std::function<void (WebsiteData)> completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    m_pendingFetchWebsiteDataCallbacks.add(callbackID, WTFMove(completionHandler));

    send(Messages::DatabaseProcess::FetchWebsiteData(sessionID, dataTypes, callbackID), 0);
}

void DatabaseProcessProxy::deleteWebsiteData(WebCore::SessionID sessionID, WebsiteDataTypes dataTypes, std::chrono::system_clock::time_point modifiedSince,  std::function<void ()> completionHandler)
{
    auto callbackID = generateCallbackID();

    m_pendingDeleteWebsiteDataCallbacks.add(callbackID, WTFMove(completionHandler));
    send(Messages::DatabaseProcess::DeleteWebsiteData(sessionID, dataTypes, modifiedSince, callbackID), 0);
}

void DatabaseProcessProxy::deleteWebsiteDataForOrigins(SessionID sessionID, WebsiteDataTypes dataTypes, const Vector<RefPtr<WebCore::SecurityOrigin>>& origins, std::function<void ()> completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    m_pendingDeleteWebsiteDataForOriginsCallbacks.add(callbackID, WTFMove(completionHandler));

    Vector<SecurityOriginData> originData;
    for (auto& origin : origins)
        originData.append(SecurityOriginData::fromSecurityOrigin(*origin));

    send(Messages::DatabaseProcess::DeleteWebsiteDataForOrigins(sessionID, dataTypes, originData, callbackID), 0);
}

void DatabaseProcessProxy::getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply> reply)
{
    m_pendingConnectionReplies.append(reply);

    if (state() == State::Launching) {
        m_numPendingConnectionRequests++;
        return;
    }

    connection()->send(Messages::DatabaseProcess::CreateDatabaseToWebProcessConnection(), 0, IPC::DispatchMessageEvenWhenWaitingForSyncReply);
}

void DatabaseProcessProxy::didClose(IPC::Connection&)
{
    // The database process must have crashed or exited, so send any pending sync replies we might have.
    while (!m_pendingConnectionReplies.isEmpty()) {
        auto reply = m_pendingConnectionReplies.takeFirst();

#if USE(UNIX_DOMAIN_SOCKETS)
        reply->send(IPC::Attachment());
#elif OS(DARWIN)
        reply->send(IPC::Attachment(0, MACH_MSG_TYPE_MOVE_SEND));
#else
        notImplemented();
#endif
    }

    for (const auto& callback : m_pendingFetchWebsiteDataCallbacks.values())
        callback(WebsiteData());
    m_pendingFetchWebsiteDataCallbacks.clear();

    for (const auto& callback : m_pendingDeleteWebsiteDataCallbacks.values())
        callback();
    m_pendingDeleteWebsiteDataCallbacks.clear();

    for (const auto& callback : m_pendingDeleteWebsiteDataForOriginsCallbacks.values())
        callback();
    m_pendingDeleteWebsiteDataForOriginsCallbacks.clear();

    // Tell ProcessPool to forget about this database process. This may cause us to be deleted.
    m_processPool->databaseProcessCrashed(this);
}

void DatabaseProcessProxy::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
}

void DatabaseProcessProxy::didCreateDatabaseToWebProcessConnection(const IPC::Attachment& connectionIdentifier)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    RefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

#if USE(UNIX_DOMAIN_SOCKETS)
    reply->send(connectionIdentifier);
#elif OS(DARWIN)
    reply->send(IPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND));
#else
    notImplemented();
#endif
}

void DatabaseProcessProxy::didFetchWebsiteData(uint64_t callbackID, const WebsiteData& websiteData)
{
    auto callback = m_pendingFetchWebsiteDataCallbacks.take(callbackID);
    callback(websiteData);
}

void DatabaseProcessProxy::didDeleteWebsiteData(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataCallbacks.take(callbackID);
    callback();
}

void DatabaseProcessProxy::didDeleteWebsiteDataForOrigins(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataForOriginsCallbacks.take(callbackID);
    callback();
}

void DatabaseProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (IPC::Connection::identifierIsNull(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        connection()->send(Messages::DatabaseProcess::CreateDatabaseToWebProcessConnection(), 0);
    
    m_numPendingConnectionRequests = 0;
}

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
