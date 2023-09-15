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

#if ENABLE(BUILT_IN_NOTIFICATIONS)

#include "MessageReceiver.h"
#include "PushMessageForTesting.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/PushSubscriptionData.h>
#include <WebCore/PushSubscriptionIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Identified.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {
namespace WebPushD {
enum class DaemonMessageType : uint8_t;
struct WebPushDaemonConnectionConfiguration;
}
}

using WebKit::WebPushD::DaemonMessageType;
using WebKit::WebPushD::PushMessageForTesting;
using WebKit::WebPushD::WebPushDaemonConnectionConfiguration;

namespace WebPushD {

class PushClientConnection : public RefCounted<PushClientConnection>, public Identified<PushClientConnection>, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<PushClientConnection> create(xpc_connection_t);

    bool hasHostAppAuditToken() const { return !!m_hostAppAuditToken; }

    WebCore::PushSubscriptionSetIdentifier subscriptionSetIdentifier();
    const String& hostAppCodeSigningIdentifier();

    bool hostAppHasPushEntitlement();
    bool hostAppHasPushInjectEntitlement();

    const String& pushPartitionString() const { return m_pushPartitionString; }
    std::optional<WTF::UUID> dataStoreIdentifier() const { return m_dataStoreIdentifier; }

    bool debugModeIsEnabled() const { return m_debugModeEnabled; }
    void setDebugModeIsEnabled(bool);

    bool useMockBundlesForTesting() const { return m_useMockBundlesForTesting; }

    void connectionClosed();

    void broadcastDebugMessage(const String&);
    void sendDebugMessage(const String&);

    void didReceiveMessageWithReplyHandler(IPC::Decoder&, Function<void(UniqueRef<IPC::Encoder>&&)>&&) override;

private:
    PushClientConnection(xpc_connection_t);

    // PushClientConnectionMessages
    void setPushAndNotificationsEnabledForOrigin(const String& originString, bool, CompletionHandler<void()>&& replySender);
    void deletePushAndNotificationRegistration(const String& originString, CompletionHandler<void(const String&)>&& replySender);
    void injectPushMessageForTesting(PushMessageForTesting&&, CompletionHandler<void(const String&)>&&);
    void injectEncryptedPushMessageForTesting(const String&, CompletionHandler<void(bool)>&&);
    void getPendingPushMessages(CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender);
    void subscribeToPushService(URL&& scopeURL, const Vector<uint8_t>& applicationServerKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& replySender);
    void unsubscribeFromPushService(URL&& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier>, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender);
    void getPushSubscription(URL&& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender);
    void incrementSilentPushCount(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);
    void removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&&);
    void removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);
    void setPublicTokenForTesting(const String& publicToken, CompletionHandler<void()>&&);
    void updateConnectionConfiguration(WebPushDaemonConnectionConfiguration&&);
    void getPushPermissionState(URL&& scopeURL, CompletionHandler<void(const Expected<uint8_t, WebCore::ExceptionData>&)>&&);
    void setHostAppAuditTokenData(const Vector<uint8_t>&);
    void getPushTopicsForTesting(CompletionHandler<void(Vector<String>, Vector<String>)>&&);
    String bundleIdentifierFromAuditToken(audit_token_t);
    bool hostHasEntitlement(ASCIILiteral);

    OSObjectPtr<xpc_connection_t> m_xpcConnection;

    std::optional<audit_token_t> m_hostAppAuditToken;
    std::optional<String> m_hostAppCodeSigningIdentifier;
    std::optional<bool> m_hostAppHasPushEntitlement;

    String m_pushPartitionString;
    Markable<WTF::UUID> m_dataStoreIdentifier;

    bool m_debugModeEnabled { false };
    bool m_useMockBundlesForTesting { false };
};

} // namespace WebPushD

#endif // ENABLE(BUILT_IN_NOTIFICATIONS)
