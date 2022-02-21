/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PushClientConnection.h"

#import "AppBundleRequest.h"
#import "CodeSigning.h"
#import "WebPushDaemon.h"
#import "WebPushDaemonConnectionConfiguration.h"
#import <JavaScriptCore/ConsoleTypes.h>
#import <wtf/HexNumber.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/Entitlements.h>

namespace WebPushD {

Ref<ClientConnection> ClientConnection::create(xpc_connection_t connection)
{
    return adoptRef(*new ClientConnection(connection));
}

ClientConnection::ClientConnection(xpc_connection_t connection)
    : m_xpcConnection(connection)
{
}

void ClientConnection::updateConnectionConfiguration(const WebPushDaemonConnectionConfiguration& configuration)
{
    if (configuration.hostAppAuditTokenData)
        setHostAppAuditTokenData(*configuration.hostAppAuditTokenData);

    m_useMockBundlesForTesting = configuration.useMockBundlesForTesting;
}

void ClientConnection::setHostAppAuditTokenData(const Vector<uint8_t>& tokenData)
{
    audit_token_t token;
    if (tokenData.size() != sizeof(token)) {
        ASSERT_WITH_MESSAGE(false, "Attempt to set an audit token from incorrect number of bytes");
        return;
    }

    memcpy(&token, tokenData.data(), tokenData.size());

    if (hasHostAppAuditToken()) {
        // Verify the token being set is equivalent to the last one set
        audit_token_t& existingAuditToken = *m_hostAppAuditToken;
        RELEASE_ASSERT(!memcmp(&existingAuditToken, &token, sizeof(token)));
        return;
    }

    m_hostAppAuditToken = WTFMove(token);
    Daemon::singleton().broadcastAllConnectionIdentities();
}

const String& ClientConnection::hostAppCodeSigningIdentifier()
{
    if (!m_hostAppCodeSigningIdentifier) {
#if PLATFORM(MAC) && !USE(APPLE_INTERNAL_SDK)
        // This isn't great, but currently the only user of webpushd in open source builds is TestWebKitAPI and codeSigningIdentifier returns the null String on x86_64 Macs.
        m_hostAppCodeSigningIdentifier = "com.apple.WebKit.TestWebKitAPI";
#else
        if (!m_hostAppAuditToken)
            m_hostAppCodeSigningIdentifier = String();
        else
            m_hostAppCodeSigningIdentifier = WebKit::codeSigningIdentifier(*m_hostAppAuditToken);
#endif
    }

    return *m_hostAppCodeSigningIdentifier;
}

bool ClientConnection::hostAppHasPushEntitlement()
{
    if (!m_hostAppHasPushEntitlement)
        m_hostAppHasPushEntitlement = hostHasEntitlement("com.apple.private.webkit.webpush"_s);

    return *m_hostAppHasPushEntitlement;
}

bool ClientConnection::hostAppHasPushInjectEntitlement()
{
    return hostHasEntitlement("com.apple.private.webkit.webpush.inject"_s);
}

bool ClientConnection::hostHasEntitlement(const char* entitlement)
{
    if (!m_hostAppAuditToken)
        return false;
#if !PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
    return WTF::hasEntitlement(*m_hostAppAuditToken, entitlement);
#else
    return true;
#endif
}

void ClientConnection::setDebugModeIsEnabled(bool enabled)
{
    if (enabled == m_debugModeEnabled)
        return;

    m_debugModeEnabled = enabled;
    broadcastDebugMessage(makeString("Turned Debug Mode ", m_debugModeEnabled ? "on" : "off"));
}

void ClientConnection::broadcastDebugMessage(const String& message)
{
    String messageIdentifier;
    auto signingIdentifer = hostAppCodeSigningIdentifier();
    if (signingIdentifer.isEmpty())
        messageIdentifier = makeString("[(0x", hex(reinterpret_cast<uint64_t>(m_xpcConnection.get()), WTF::HexConversionMode::Lowercase), ") (", String::number(identifier()), " )] ");
    else
        messageIdentifier = makeString("[", signingIdentifer, " (", String::number(identifier()), ")] ");

    Daemon::singleton().broadcastDebugMessage(JSC::MessageLevel::Info, makeString(messageIdentifier, message));
}

void ClientConnection::enqueueAppBundleRequest(std::unique_ptr<AppBundleRequest>&& request)
{
    RELEASE_ASSERT(m_xpcConnection);
    m_pendingBundleRequests.append(WTFMove(request));
    maybeStartNextAppBundleRequest();
}

void ClientConnection::maybeStartNextAppBundleRequest()
{
    RELEASE_ASSERT(m_xpcConnection);

    if (m_currentBundleRequest || m_pendingBundleRequests.isEmpty())
        return;

    m_currentBundleRequest = m_pendingBundleRequests.takeFirst();
    m_currentBundleRequest->start();
}

void ClientConnection::didCompleteAppBundleRequest(AppBundleRequest& request)
{
    // If our connection was closed there should be no in-progress bundle requests.
    RELEASE_ASSERT(m_xpcConnection);

    ASSERT(m_currentBundleRequest.get() == &request);
    m_currentBundleRequest = nullptr;

    maybeStartNextAppBundleRequest();
}

void ClientConnection::connectionClosed()
{
    broadcastDebugMessage("Connection closed");

    RELEASE_ASSERT(m_xpcConnection);
    m_xpcConnection = nullptr;

    if (m_currentBundleRequest) {
        m_currentBundleRequest->cancel();
        m_currentBundleRequest = nullptr;
    }

    Deque<std::unique_ptr<AppBundleRequest>> pendingBundleRequests;
    pendingBundleRequests.swap(m_pendingBundleRequests);
    for (auto& requst : pendingBundleRequests)
        requst->cancel();
}

} // namespace WebPushD
