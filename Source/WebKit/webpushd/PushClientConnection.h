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

#if ENABLE(WEB_PUSH_NOTIFICATIONS)

#include "Connection.h"
#include "MessageReceiver.h"
#include "PushMessageForTesting.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/NotificationResources.h>
#include <WebCore/PushPermissionState.h>
#include <WebCore/PushSubscriptionData.h>
#include <WebCore/PushSubscriptionIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Identified.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS UIWebClip;

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

enum class PushClientConnectionIdentifierType { };
using PushClientConnectionIdentifier = AtomicObjectIdentifier<PushClientConnectionIdentifierType>;

class PushClientConnection : public RefCounted<PushClientConnection>, public Identified<PushClientConnectionIdentifier>, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(PushClientConnection);
public:
    static RefPtr<PushClientConnection> create(xpc_connection_t, IPC::Decoder&);

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    std::optional<WebCore::PushSubscriptionSetIdentifier> subscriptionSetIdentifierForOrigin(const WebCore::SecurityOriginData&) const;
    const String& hostAppCodeSigningIdentifier() const { return m_hostAppCodeSigningIdentifier; }
    bool hostAppHasPushInjectEntitlement() const { return m_hostAppHasPushInjectEntitlement; };
    std::optional<WTF::UUID> dataStoreIdentifier() const { return m_dataStoreIdentifier; }

    // You almost certainly do not want to use this and should probably use subscriptionSetIdentifierForOrigin instead.
    const String& pushPartitionIfExists() const { return m_pushPartitionString; }

    String debugDescription() const;

    void connectionClosed();

    void didReceiveMessageWithReplyHandler(IPC::Decoder&, Function<void(UniqueRef<IPC::Encoder>&&)>&&) override;

private:
    PushClientConnection(xpc_connection_t, String&& hostAppCodeSigningIdentifier, bool hostAppHasPushInjectEntitlement, String&& pushPartitionString, std::optional<WTF::UUID>&& dataStoreIdentifier);

    // PushClientConnectionMessages
    void setPushAndNotificationsEnabledForOrigin(const String& originString, bool, CompletionHandler<void()>&& replySender);
    void injectPushMessageForTesting(PushMessageForTesting&&, CompletionHandler<void(const String&)>&&);
    void injectEncryptedPushMessageForTesting(const String&, CompletionHandler<void(bool)>&&);
    void getPendingPushMessage(CompletionHandler<void(const std::optional<WebKit::WebPushMessage>&)>&& replySender);
    void getPendingPushMessages(CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender);
    void subscribeToPushService(URL&& scopeURL, const Vector<uint8_t>& applicationServerKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& replySender);
    void unsubscribeFromPushService(URL&& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier>, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender);
    void getPushSubscription(URL&& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender);
    void incrementSilentPushCount(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);
    void removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&&);
    void removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&&, CompletionHandler<void(unsigned)>&&);
    void setPublicTokenForTesting(const String& publicToken, CompletionHandler<void()>&&);
    void initializeConnection(WebPushDaemonConnectionConfiguration&&);
    void getPushPermissionState(WebCore::SecurityOriginData&&, CompletionHandler<void(WebCore::PushPermissionState)>&&);
    void requestPushPermission(WebCore::SecurityOriginData&&, CompletionHandler<void(bool)>&&);
    void setHostAppAuditTokenData(const Vector<uint8_t>&);
    void getPushTopicsForTesting(CompletionHandler<void(Vector<String>, Vector<String>)>&&);

    void showNotification(const WebCore::NotificationData&, RefPtr<WebCore::NotificationResources>, CompletionHandler<void()>&&);
    void getNotifications(const URL& registrationURL, const String& tag, CompletionHandler<void(Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&&)>&&);
    void cancelNotification(WebCore::SecurityOriginData&&, const WTF::UUID& notificationID);
    void setAppBadge(WebCore::SecurityOriginData&&, std::optional<uint64_t>);
    void getAppBadgeForTesting(CompletionHandler<void(std::optional<uint64_t>)>&&);
    void setProtocolVersionForTesting(unsigned, CompletionHandler<void()>&&);

    OSObjectPtr<xpc_connection_t> m_xpcConnection;
    String m_hostAppCodeSigningIdentifier;
    bool m_hostAppHasPushInjectEntitlement { false };
    String m_pushPartitionString;
    Markable<WTF::UUID> m_dataStoreIdentifier;
};

} // namespace WebPushD

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
