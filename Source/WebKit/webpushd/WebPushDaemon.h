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

#include "PushClientConnection.h"
#include "PushMessageForTesting.h"
#include "PushService.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushDaemonConstants.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/PushSubscriptionData.h>
#include <WebCore/Timer.h>
#include <span>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>


namespace JSC {
enum class MessageLevel : uint8_t;
}

namespace WebCore {
class SecurityOriginData;
}

using WebKit::WebPushD::PushMessageForTesting;
using WebKit::WebPushD::WebPushDaemonConnectionConfiguration;

namespace WebPushD {

using EncodedMessage = Vector<uint8_t>;

class WebPushDaemon {
    friend class WTF::NeverDestroyed<WebPushDaemon>;
public:
    static WebPushDaemon& singleton();

    void connectionEventHandler(xpc_object_t);
    void connectionAdded(xpc_connection_t);
    void connectionRemoved(xpc_connection_t);

    void setMachServiceName(const String& machServiceName) { m_machServiceName = machServiceName; }
    void startMockPushService();
    void startPushService(const String& incomingPushServiceName, const String& pushDatabasePath);
    void handleIncomingPush(const WebCore::PushSubscriptionSetIdentifier&, WebKit::WebPushMessage&&);

    // Message handlers
    void setPushAndNotificationsEnabledForOrigin(PushClientConnection&, const String& originString, bool, CompletionHandler<void()>&& replySender);
    void deletePushAndNotificationRegistration(PushClientConnection&, const String& originString, CompletionHandler<void(const String&)>&& replySender);
    void injectPushMessageForTesting(PushClientConnection&, PushMessageForTesting&&, CompletionHandler<void(const String&)>&&);
    void injectEncryptedPushMessageForTesting(PushClientConnection&, const String&, CompletionHandler<void(bool)>&&);
    void getPendingPushMessages(PushClientConnection&, CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender);
    void getPushTopicsForTesting(CompletionHandler<void(Vector<String>, Vector<String>)>&&);
    void subscribeToPushService(PushClientConnection&, const URL& scopeURL, const Vector<uint8_t>& applicationServerKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& replySender);
    void unsubscribeFromPushService(PushClientConnection&, const URL& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier>, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender);
    void getPushSubscription(PushClientConnection&, const URL& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender);
    void incrementSilentPushCount(PushClientConnection&, const WebCore::SecurityOriginData&, CompletionHandler<void(unsigned)>&&);
    void removeAllPushSubscriptions(PushClientConnection&, CompletionHandler<void(unsigned)>&&);
    void removePushSubscriptionsForOrigin(PushClientConnection&, const WebCore::SecurityOriginData&, CompletionHandler<void(unsigned)>&&);
    void setPublicTokenForTesting(const String& publicToken, CompletionHandler<void()>&&);

    void broadcastDebugMessage(const String&);
    void broadcastAllConnectionIdentities();

private:
    WebPushDaemon();

    bool canRegisterForNotifications(PushClientConnection&);

    void notifyClientPushMessageIsAvailable(const WebCore::PushSubscriptionSetIdentifier&);

    void setPushService(std::unique_ptr<PushService>&&);
    void runAfterStartingPushService(Function<void()>&&);

    void ensureIncomingPushTransaction();
    void releaseIncomingPushTransaction();
    void incomingPushTransactionTimerFired();

    void deletePushRegistration(const WebCore::PushSubscriptionSetIdentifier&, const String&, CompletionHandler<void()>&&);

    PushClientConnection* toPushClientConnection(xpc_connection_t);
    HashMap<xpc_connection_t, Ref<PushClientConnection>> m_connectionMap;

    std::unique_ptr<PushService> m_pushService;
    bool m_pushServiceStarted { false };
    Deque<Function<void()>> m_pendingPushServiceFunctions;

    HashMap<WebCore::PushSubscriptionSetIdentifier, Vector<WebKit::WebPushMessage>> m_pushMessages;
    HashMap<String, Deque<PushMessageForTesting>> m_testingPushMessages;
    
    WebCore::Timer m_incomingPushTransactionTimer;
    OSObjectPtr<os_transaction_t> m_incomingPushTransaction;
    String m_machServiceName;
};

} // namespace WebPushD

#endif // ENABLE(BUILT_IN_NOTIFICATIONS)

