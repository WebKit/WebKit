/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "NetworkSocketChannel.h"

#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "WebSocketChannelMessages.h"
#include "WebSocketTask.h"
#include <WebCore/LocalNetworkAccess.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ThreadableWebSocketChannel.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include "NetworkSessionCocoa.h"
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkSocketChannel);

RefPtr<NetworkSocketChannel> NetworkSocketChannel::create(NetworkConnectionToWebProcess& connection, PAL::SessionID sessionID, const ResourceRequest& request, const String& protocol, WebSocketIdentifier identifier, WebPageProxyIdentifier webPageProxyID, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, const WebCore::ClientOrigin& clientOrigin, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, IsInitiatedByWorker isInitiatedByWorker, LocalNetworkAccessClaims&& localNetworkAccessClaims)
{
    Ref result = adoptRef(*new NetworkSocketChannel(connection, protect(NetworkProcess::singleton().networkSession(sessionID)), request, protocol, identifier, webPageProxyID, frameID, pageID, clientOrigin, hadMainFrameMainResourcePrivateRelayed, allowPrivacyProxy, advancedPrivacyProtections, storedCredentialsPolicy, isInitiatedByWorker, WTF::move(localNetworkAccessClaims)));
    if (!result->m_socket && !result->m_isAwaitingLocalNetworkAccessBeforeConnecting && !result->m_localNetworkAccessRefused) {
        result->didClose(0, "Cannot create a web socket task"_s);
        return nullptr;
    }
    return result;
}

NetworkSocketChannel::NetworkSocketChannel(NetworkConnectionToWebProcess& connection, NetworkSession* session, const ResourceRequest& request, const String& protocol, WebSocketIdentifier identifier, WebPageProxyIdentifier webPageProxyID, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, const WebCore::ClientOrigin& clientOrigin, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, IsInitiatedByWorker isInitiatedByWorker, LocalNetworkAccessClaims&& localNetworkAccessClaims)
    : m_connectionToWebProcess(connection)
    , m_identifier(identifier)
    , m_session(session)
    , m_errorTimer(*this, &NetworkSocketChannel::sendDelayedError)
    , m_webPageProxyID(webPageProxyID)
    , m_url(request.url())
    , m_frameID(frameID)
    , m_clientOrigin(clientOrigin)
    , m_localNetworkAccessClaims(WTF::move(localNetworkAccessClaims))
{
    if (!session)
        return;

    m_connect = [weakThis = WeakPtr { *this }, webPageProxyID, frameID, pageID, request = ResourceRequest { request }, protocol, clientOrigin, hadMainFrameMainResourcePrivateRelayed, allowPrivacyProxy, advancedPrivacyProtections, storedCredentialsPolicy, isInitiatedByWorker] {
        RefPtr protectedThis = weakThis;
        if (!protectedThis)
            return;
        CheckedPtr session = protectedThis->m_session;
        if (!session)
            return;
        protectedThis->m_socket = session->createWebSocketTask(webPageProxyID, frameID, pageID, *protectedThis, request, protocol, clientOrigin, hadMainFrameMainResourcePrivateRelayed, allowPrivacyProxy, advancedPrivacyProtections, storedCredentialsPolicy, isInitiatedByWorker);
        if (RefPtr socket = protectedThis->m_socket) {
#if PLATFORM(COCOA)
            session->addWebSocketTask(*socket);
#endif
            socket->resume();
        }
    };

    auto addressSpace = canDetermineConnectionAddressSpace() && connection.localNetworkAccessEnabled() ? session->addressSpaceBeforeConnecting(request.url()) : std::nullopt;
    if (!addressSpace) {
        beginConnecting();
        return;
    }

    m_isAwaitingLocalNetworkAccessBeforeConnecting = true;
    checkLocalNetworkAccess(*addressSpace, [weakThis = WeakPtr { *this }, addressSpace = *addressSpace](const String& errorMessage) {
        RefPtr protectedThis = weakThis;
        if (!protectedThis || !protectedThis->m_isAwaitingLocalNetworkAccessBeforeConnecting)
            return;
        protectedThis->m_isAwaitingLocalNetworkAccessBeforeConnecting = false;
        if (!errorMessage.isNull())
            return protectedThis->refuseLocalNetworkAccess(errorMessage);
        protectedThis->m_addressSpaceCheckedBeforeConnecting = addressSpace;
        protectedThis->beginConnecting();
    });
}

void NetworkSocketChannel::beginConnecting()
{
    if (auto connect = std::exchange(m_connect, nullptr))
        connect();
}

NetworkSocketChannel::~NetworkSocketChannel()
{
    if (RefPtr socket = m_socket.get()) {
#if PLATFORM(COCOA)
        if (m_session)
            CheckedRef { *m_session }->removeWebSocketTask(*socket);
#endif
        socket->cancel();
    }
}

void NetworkSocketChannel::sendString(std::span<const uint8_t> message, CompletionHandler<void()>&& callback)
{
    if (RefPtr socket = m_socket)
        socket->sendString(message, WTF::move(callback));
    else
        callback();
}

void NetworkSocketChannel::sendData(std::span<const uint8_t> data, CompletionHandler<void()>&& callback)
{
    if (RefPtr socket = m_socket)
        socket->sendData(data, WTF::move(callback));
    else
        callback();
}

void NetworkSocketChannel::finishClosingIfPossible()
{
    if (m_state == State::Open) {
        m_state = State::Closing;
        return;
    }
    ASSERT(m_state == State::Closing);
    m_state = State::Closed;
    protect(m_connectionToWebProcess)->removeSocketChannel(m_identifier);
}

void NetworkSocketChannel::close(int32_t code, const String& reason)
{
    if (RefPtr socket = m_socket)
        socket->close(code, reason);
    else {
        m_connect = nullptr;
        m_isAwaitingLocalNetworkAccessBeforeConnecting = false;
        didClose(ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure, emptyString());
    }
    finishClosingIfPossible();
}

void NetworkSocketChannel::didConnect(const String& subprotocol, const String& extensions, IPAddressSpace connectionAddressSpace)
{
    if (m_addressSpaceCheckedBeforeConnecting == connectionAddressSpace) {
        send(Messages::WebSocketChannel::DidConnect { subprotocol, extensions });
        return;
    }

    m_messagesAwaitingLocalNetworkAccess.emplace();
    checkLocalNetworkAccess(connectionAddressSpace, [weakThis = WeakPtr { *this }, subprotocol, extensions](const String& errorMessage) {
        RefPtr protectedThis = weakThis;
        if (!protectedThis)
            return;
        auto messages = std::exchange(protectedThis->m_messagesAwaitingLocalNetworkAccess, std::nullopt);
        if (!errorMessage.isNull())
            return protectedThis->refuseLocalNetworkAccess(errorMessage);
        protectedThis->send(Messages::WebSocketChannel::DidConnect { subprotocol, extensions });
        for (auto& message : *messages)
            message();
    });
}

void NetworkSocketChannel::checkLocalNetworkAccess(IPAddressSpace connectionAddressSpace, CompletionHandler<void(const String& errorMessage)>&& completionHandler)
{
    Ref connection = m_connectionToWebProcess;
    CheckedPtr session = m_session;
    if (!canDetermineConnectionAddressSpace() || !connection->localNetworkAccessEnabled() || !session)
        return completionHandler(String { });

    // The handshake is a fetch of the http(s) URL, so that is the URL the check judges.
    URL url = m_url;
    if (url.protocolIs("ws"_s))
        url.setProtocol("http"_s);
    else if (url.protocolIs("wss"_s))
        url.setProtocol("https"_s);

    auto& claims = m_localNetworkAccessClaims;
    auto client = connection->localNetworkAccessClient(m_frameID, claims.parentFrameID, m_clientOrigin, claims.clientAddressSpace, claims.clientIsSecureContext);
    auto requirement = WebCore::checkLocalNetworkAccess(ResourceRequest { URL { url } }, url, connectionAddressSpace, client.addressSpace, client.isSecureContext, client.origin, claims.localNetworkAllowedByPermissionsPolicy, claims.loopbackNetworkAllowedByPermissionsPolicy);
    if (!requirement)
        return completionHandler(requirement.error().localizedDescription());
    if (*requirement == LocalNetworkAccessRequirement::None)
        return completionHandler(String { });

    session->requestLocalNetworkAccessPermission(m_webPageProxyID, client.origin, connectionAddressSpace, [url = WTF::move(url), completionHandler = WTF::move(completionHandler)](PermissionState state) mutable {
        auto error = localNetworkAccessPermissionError(url, state);
        completionHandler(error ? error->localizedDescription() : String { });
    });
}

bool NetworkSocketChannel::isHeldForLocalNetworkAccess() const
{
    return m_localNetworkAccessRefused || m_messagesAwaitingLocalNetworkAccess;
}

void NetworkSocketChannel::holdForLocalNetworkAccess(Function<void()>&& message)
{
    if (m_messagesAwaitingLocalNetworkAccess)
        m_messagesAwaitingLocalNetworkAccess->append(WTF::move(message));
}

void NetworkSocketChannel::refuseLocalNetworkAccess(const String& errorMessage)
{
    m_localNetworkAccessRefused = true;
    if (RefPtr socket = m_socket)
        socket->cancel();
    m_errorMessage = makeString("Blocked a local network connection because "_s, errorMessage, "."_s);
    m_closeInfo = std::make_pair(ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure, emptyString());
    m_errorTimer.startOneShot(NetworkProcess::randomClosedPortDelay());
}

void NetworkSocketChannel::didReceiveText(const String& text)
{
    if (isHeldForLocalNetworkAccess()) {
        holdForLocalNetworkAccess([this, protectedThis = Ref { *this }, text] {
            didReceiveText(text);
        });
        return;
    }
    send(Messages::WebSocketChannel::DidReceiveText { text });
}

void NetworkSocketChannel::didReceiveBinaryData(std::span<const uint8_t> data)
{
    if (isHeldForLocalNetworkAccess()) {
        holdForLocalNetworkAccess([this, protectedThis = Ref { *this }, data = Vector<uint8_t> { data }] {
            didReceiveBinaryData(data.span());
        });
        return;
    }
    send(Messages::WebSocketChannel::DidReceiveBinaryData { data });
}

void NetworkSocketChannel::didClose(unsigned short code, const String& reason)
{
    if (isHeldForLocalNetworkAccess()) {
        holdForLocalNetworkAccess([this, protectedThis = Ref { *this }, code, reason] {
            didClose(code, reason);
        });
        return;
    }
    if (m_errorTimer.isActive()) {
        m_closeInfo = std::make_pair(code, reason);
        return;
    }
    send(Messages::WebSocketChannel::DidClose { code, reason });
    finishClosingIfPossible();
}

void NetworkSocketChannel::didReceiveMessageError(String&& errorMessage)
{
    if (isHeldForLocalNetworkAccess()) {
        holdForLocalNetworkAccess([this, protectedThis = Ref { *this }, errorMessage = WTF::move(errorMessage)] mutable {
            didReceiveMessageError(WTF::move(errorMessage));
        });
        return;
    }
    m_errorMessage = WTF::move(errorMessage);
    m_errorTimer.startOneShot(NetworkProcess::randomClosedPortDelay());
}

void NetworkSocketChannel::sendDelayedError()
{
    send(Messages::WebSocketChannel::DidReceiveMessageError { m_errorMessage });
    if (m_closeInfo) {
        send(Messages::WebSocketChannel::DidClose { m_closeInfo->first, m_closeInfo->second });
        finishClosingIfPossible();
    }
}

void NetworkSocketChannel::didSendHandshakeRequest(ResourceRequest&& request)
{
    send(Messages::WebSocketChannel::DidSendHandshakeRequest { request });
}

void NetworkSocketChannel::didReceiveHandshakeResponse(ResourceResponse&& response)
{
    if (isHeldForLocalNetworkAccess()) {
        holdForLocalNetworkAccess([this, protectedThis = Ref { *this }, response = WTF::move(response)] mutable {
            didReceiveHandshakeResponse(WTF::move(response));
        });
        return;
    }
    response.sanitizeHTTPHeaderFields(ResourceResponse::SanitizationType::CrossOriginSafe);
    send(Messages::WebSocketChannel::DidReceiveHandshakeResponse { response });
}

IPC::Connection* NetworkSocketChannel::messageSenderConnection() const
{
    return &m_connectionToWebProcess->connection();
}

NetworkSession* NetworkSocketChannel::session() const
{
    return m_session.get();
}

std::optional<SharedPreferencesForWebProcess> NetworkSocketChannel::sharedPreferencesForWebProcess() const
{
    auto* connectionToWebProcess = &m_connectionToWebProcess.get();
    if (!connectionToWebProcess)
        return std::nullopt;

    return connectionToWebProcess->sharedPreferencesForWebProcess();
}

} // namespace WebKit
