/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "NetworkProcessProxy.h"

#include "AuthenticationChallengeProxy.h"
#include "CustomProtocolManagerProxyMessages.h"
#include "DatabaseProcessMessages.h"
#include "DownloadProxyMessages.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "SandboxExtension.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebsiteData.h"
#include <wtf/RunLoop.h>

#if ENABLE(SEC_ITEM_SHIM)
#include "SecItemShimProxy.h"
#endif

#if PLATFORM(IOS)
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())

using namespace WebCore;

namespace WebKit {

static uint64_t generateCallbackID()
{
    static uint64_t callbackID;

    return ++callbackID;
}

Ref<NetworkProcessProxy> NetworkProcessProxy::create(WebProcessPool& processPool)
{
    return adoptRef(*new NetworkProcessProxy(processPool));
}

NetworkProcessProxy::NetworkProcessProxy(WebProcessPool& processPool)
    : m_processPool(processPool)
    , m_numPendingConnectionRequests(0)
    , m_customProtocolManagerProxy(this, processPool)
    , m_throttler(*this)
{
    connect();
}

NetworkProcessProxy::~NetworkProcessProxy()
{
    ASSERT(m_pendingFetchWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataCallbacks.isEmpty());
    ASSERT(m_pendingDeleteWebsiteDataForOriginsCallbacks.isEmpty());
}

void NetworkProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Network;
    ChildProcessProxy::getLaunchOptions(launchOptions);
}

void NetworkProcessProxy::connectionWillOpen(IPC::Connection& connection)
{
#if ENABLE(SEC_ITEM_SHIM)
    SecItemShimProxy::singleton().initializeConnection(connection);
#else
    UNUSED_PARAM(connection);
#endif
}

void NetworkProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

void NetworkProcessProxy::getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply)
{
    m_pendingConnectionReplies.append(reply);

    if (state() == State::Launching) {
        m_numPendingConnectionRequests++;
        return;
    }

    connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(), 0, IPC::DispatchMessageEvenWhenWaitingForSyncReply);
}

DownloadProxy* NetworkProcessProxy::createDownloadProxy(const ResourceRequest& resourceRequest)
{
    if (!m_downloadProxyMap)
        m_downloadProxyMap = std::make_unique<DownloadProxyMap>(this);

    return m_downloadProxyMap->createDownloadProxy(m_processPool, resourceRequest);
}

void NetworkProcessProxy::fetchWebsiteData(SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, std::function<void (WebsiteData)> completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    auto token = throttler().backgroundActivityToken();

    m_pendingFetchWebsiteDataCallbacks.add(callbackID, [token, completionHandler](WebsiteData websiteData) {
        completionHandler(WTFMove(websiteData));
    });

    send(Messages::NetworkProcess::FetchWebsiteData(sessionID, dataTypes, fetchOptions, callbackID), 0);
}

void NetworkProcessProxy::deleteWebsiteData(WebCore::SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, std::chrono::system_clock::time_point modifiedSince,  std::function<void ()> completionHandler)
{
    auto callbackID = generateCallbackID();
    auto token = throttler().backgroundActivityToken();

    m_pendingDeleteWebsiteDataCallbacks.add(callbackID, [token, completionHandler] {
        completionHandler();
    });
    send(Messages::NetworkProcess::DeleteWebsiteData(sessionID, dataTypes, modifiedSince, callbackID), 0);
}

void NetworkProcessProxy::deleteWebsiteDataForOrigins(SessionID sessionID, OptionSet<WebsiteDataType> dataTypes, const Vector<RefPtr<WebCore::SecurityOrigin>>& origins, const Vector<String>& cookieHostNames, std::function<void ()> completionHandler)
{
    ASSERT(canSendMessage());

    uint64_t callbackID = generateCallbackID();
    auto token = throttler().backgroundActivityToken();

    m_pendingDeleteWebsiteDataForOriginsCallbacks.add(callbackID, [token, completionHandler] {
        completionHandler();
    });

    Vector<SecurityOriginData> originData;
    for (auto& origin : origins)
        originData.append(SecurityOriginData::fromSecurityOrigin(*origin));

    send(Messages::NetworkProcess::DeleteWebsiteDataForOrigins(sessionID, dataTypes, originData, cookieHostNames, callbackID), 0);
}

void NetworkProcessProxy::networkProcessCrashedOrFailedToLaunch()
{
    // The network process must have crashed or exited, send any pending sync replies we might have.
    while (!m_pendingConnectionReplies.isEmpty()) {
        RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

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

    // Tell the network process manager to forget about this network process proxy. This may cause us to be deleted.
    m_processPool.networkProcessCrashed(this);
}

void NetworkProcessProxy::didReceiveMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    if (m_processPool.dispatchMessage(connection, decoder))
        return;

    didReceiveNetworkProcessProxyMessage(connection, decoder);
}

void NetworkProcessProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::MessageDecoder& decoder, std::unique_ptr<IPC::MessageEncoder>& replyEncoder)
{
    if (dispatchSyncMessage(connection, decoder, replyEncoder))
        return;

    ASSERT_NOT_REACHED();
}

void NetworkProcessProxy::didClose(IPC::Connection&)
{
    if (m_downloadProxyMap)
        m_downloadProxyMap->processDidClose();

    m_tokenForHoldingLockedFiles = nullptr;

    // This may cause us to be deleted.
    networkProcessCrashedOrFailedToLaunch();
}

void NetworkProcessProxy::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference)
{
}

void NetworkProcessProxy::didCreateNetworkConnectionToWebProcess(const IPC::Attachment& connectionIdentifier)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    // Grab the first pending connection reply.
    RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

#if USE(UNIX_DOMAIN_SOCKETS)
    reply->send(connectionIdentifier);
#elif OS(DARWIN)
    reply->send(IPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND));
#else
    notImplemented();
#endif
}

void NetworkProcessProxy::didReceiveAuthenticationChallenge(uint64_t pageID, uint64_t frameID, const WebCore::AuthenticationChallenge& coreChallenge, uint64_t challengeID)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    MESSAGE_CHECK(page);

    RefPtr<AuthenticationChallengeProxy> authenticationChallenge = AuthenticationChallengeProxy::create(coreChallenge, challengeID, connection());
    page->didReceiveAuthenticationChallengeProxy(frameID, authenticationChallenge.release());
}

void NetworkProcessProxy::didFetchWebsiteData(uint64_t callbackID, const WebsiteData& websiteData)
{
    auto callback = m_pendingFetchWebsiteDataCallbacks.take(callbackID);
    callback(websiteData);
}

void NetworkProcessProxy::didDeleteWebsiteData(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataCallbacks.take(callbackID);
    callback();
}

void NetworkProcessProxy::didDeleteWebsiteDataForOrigins(uint64_t callbackID)
{
    auto callback = m_pendingDeleteWebsiteDataForOriginsCallbacks.take(callbackID);
    callback();
}

void NetworkProcessProxy::grantSandboxExtensionsToDatabaseProcessForBlobs(uint64_t requestID, const Vector<String>& paths)
{
    SandboxExtension::HandleArray extensions;
    extensions.allocate(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        // ReadWrite is required for creating hard links as well as deleting the temporary file, which the DatabaseProcess will do.
        SandboxExtension::createHandle(paths[i], SandboxExtension::ReadWrite, extensions[i]);
    }

    m_processPool.sendToDatabaseProcessRelaunchingIfNecessary(Messages::DatabaseProcess::GrantSandboxExtensionsForBlobs(paths, extensions));
    connection()->send(Messages::NetworkProcess::DidGrantSandboxExtensionsToDatabaseProcessForBlobs(requestID), 0);
}

void NetworkProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (IPC::Connection::identifierIsNull(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(), 0);
    
    m_numPendingConnectionRequests = 0;

#if PLATFORM(COCOA)
    if (m_processPool.processSuppressionEnabled())
        setProcessSuppressionEnabled(true);
#endif
    
#if PLATFORM(IOS)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_throttler.didConnectToProcess(xpc_connection_get_pid(connection));
#endif
}

void NetworkProcessProxy::logSampledDiagnosticMessage(uint64_t pageID, const String& message, const String& description)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logSampledDiagnosticMessage(message, description);
}

void NetworkProcessProxy::logSampledDiagnosticMessageWithResult(uint64_t pageID, const String& message, const String& description, uint32_t result)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logSampledDiagnosticMessageWithResult(message, description, result);
}

void NetworkProcessProxy::logSampledDiagnosticMessageWithValue(uint64_t pageID, const String& message, const String& description, const String& value)
{
    WebPageProxy* page = WebProcessProxy::webPage(pageID);
    // FIXME: We do this null-check because by the time the decision to log is made, the page may be gone. We should refactor to avoid this,
    // but for now we simply drop the message in the rare case this happens.
    if (!page)
        return;

    page->logSampledDiagnosticMessageWithValue(message, description, value);
}

void NetworkProcessProxy::sendProcessWillSuspendImminently()
{
    if (!canSendMessage())
        return;

    bool handled = false;
    sendSync(Messages::NetworkProcess::ProcessWillSuspendImminently(), Messages::NetworkProcess::ProcessWillSuspendImminently::Reply(handled), 0, std::chrono::seconds(1));
}
    
void NetworkProcessProxy::sendPrepareToSuspend()
{
    if (canSendMessage())
        send(Messages::NetworkProcess::PrepareToSuspend(), 0);
}

void NetworkProcessProxy::sendCancelPrepareToSuspend()
{
    if (canSendMessage())
        send(Messages::NetworkProcess::CancelPrepareToSuspend(), 0);
}

void NetworkProcessProxy::sendProcessDidResume()
{
    if (canSendMessage())
        send(Messages::NetworkProcess::ProcessDidResume(), 0);
}

void NetworkProcessProxy::processReadyToSuspend()
{
    m_throttler.processReadyToSuspend();
}

void NetworkProcessProxy::didSetAssertionState(AssertionState)
{
}
    
void NetworkProcessProxy::setIsHoldingLockedFiles(bool isHoldingLockedFiles)
{
    if (!isHoldingLockedFiles) {
        m_tokenForHoldingLockedFiles = nullptr;
        return;
    }
    if (!m_tokenForHoldingLockedFiles)
        m_tokenForHoldingLockedFiles = m_throttler.backgroundActivityToken();
}

} // namespace WebKit
