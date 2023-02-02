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

#pragma once

#include <WebCore/PushSubscriptionIdentifier.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Identified.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
namespace WebPushD {
enum class DaemonMessageType : uint8_t;
struct WebPushDaemonConnectionConfiguration;
}
}

using WebKit::WebPushD::DaemonMessageType;
using WebKit::WebPushD::WebPushDaemonConnectionConfiguration;

namespace WebPushD {

class AppBundleRequest;

class ClientConnection : public RefCounted<ClientConnection>, public CanMakeWeakPtr<ClientConnection>, public Identified<ClientConnection> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ClientConnection> create(xpc_connection_t);

    void updateConnectionConfiguration(const WebPushDaemonConnectionConfiguration&);

    bool hasHostAppAuditToken() const { return !!m_hostAppAuditToken; }

    WebCore::PushSubscriptionSetIdentifier subscriptionSetIdentifier();

    const String& hostAppCodeSigningIdentifier();
    bool hostAppHasPushEntitlement();
    bool hostAppHasPushInjectEntitlement();

    const String& pushPartitionString() const { return m_pushPartitionString; }
    std::optional<UUID> dataStoreIdentifier() const { return m_dataStoreIdentifier; }

    bool debugModeIsEnabled() const { return m_debugModeEnabled; }
    void setDebugModeIsEnabled(bool);

    bool useMockBundlesForTesting() const { return m_useMockBundlesForTesting; }

    void enqueueAppBundleRequest(std::unique_ptr<AppBundleRequest>&&);
    void didCompleteAppBundleRequest(AppBundleRequest&);

    void connectionClosed();

    void broadcastDebugMessage(const String&);
    void sendDebugMessage(const String&);

private:
    ClientConnection(xpc_connection_t);

    void maybeStartNextAppBundleRequest();
    void setHostAppAuditTokenData(const Vector<uint8_t>&);

    bool hostHasEntitlement(ASCIILiteral);

    template<DaemonMessageType messageType, typename... Args>
    void sendDaemonMessage(Args&&...) const;

    OSObjectPtr<xpc_connection_t> m_xpcConnection;

    std::optional<audit_token_t> m_hostAppAuditToken;
    std::optional<String> m_hostAppCodeSigningIdentifier;
    std::optional<bool> m_hostAppHasPushEntitlement;

    String m_pushPartitionString;
    Markable<UUID> m_dataStoreIdentifier;

    Deque<std::unique_ptr<AppBundleRequest>> m_pendingBundleRequests;
    std::unique_ptr<AppBundleRequest> m_currentBundleRequest;

    bool m_debugModeEnabled { false };
    bool m_useMockBundlesForTesting { false };
};

} // namespace WebPushD
