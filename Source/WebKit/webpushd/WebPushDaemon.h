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

#include "PushClientConnection.h"
#include "PushMessageForTesting.h"
#include "PushService.h"
#include "WebPushDaemonConnectionConfiguration.h"
#include "WebPushDaemonConstants.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/PushPermissionState.h>
#include <WebCore/PushSubscriptionData.h>
#include <WebCore/Timer.h>
#include <span>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/StdList.h>
#include <wtf/spi/darwin/XPCSPI.h>

#if PLATFORM(IOS)
#include "WebClipCache.h"

@class FBSOpenApplicationService;
#endif

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

    // Do nothing since this is a singleton.
    void ref() const { }
    void deref() const { }

    void connectionEventHandler(xpc_object_t);
    void connectionAdded(xpc_connection_t);
    void connectionRemoved(xpc_connection_t);

    void startMockPushService();
    void startPushService(const String& incomingPushServiceName, const String& pushDatabasePath, const String& webClipCachePath);
    void handleIncomingPush(const WebCore::PushSubscriptionSetIdentifier&, WebKit::WebPushMessage&&);

#if PLATFORM(IOS)
    WebClipCache& ensureWebClipCache();
#endif

    // Message handlers
    void setPushAndNotificationsEnabledForOrigin(PushClientConnection&, const String& originString, bool, CompletionHandler<void()>&& replySender);
    void injectPushMessageForTesting(PushClientConnection&, PushMessageForTesting&&, CompletionHandler<void(const String&)>&&);
    void injectEncryptedPushMessageForTesting(PushClientConnection&, const String&, CompletionHandler<void(bool)>&&);
    void getPendingPushMessage(PushClientConnection&, CompletionHandler<void(const std::optional<WebKit::WebPushMessage>&)>&& replySender);
    void getPendingPushMessages(PushClientConnection&, CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender);
    void getPushTopicsForTesting(PushClientConnection&, CompletionHandler<void(Vector<String>, Vector<String>)>&&);
    void subscribeToPushService(PushClientConnection&, const URL& scopeURL, const Vector<uint8_t>& applicationServerKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& replySender);
    void unsubscribeFromPushService(PushClientConnection&, const URL& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier>, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender);
    void getPushSubscription(PushClientConnection&, const URL& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender);
    void incrementSilentPushCount(PushClientConnection&, const WebCore::SecurityOriginData&, CompletionHandler<void(unsigned)>&&);
    void removeAllPushSubscriptions(PushClientConnection&, CompletionHandler<void(unsigned)>&&);
    void removePushSubscriptionsForOrigin(PushClientConnection&, const WebCore::SecurityOriginData&, CompletionHandler<void(unsigned)>&&);
    void setPublicTokenForTesting(PushClientConnection&, const String& publicToken, CompletionHandler<void()>&&);

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    void showNotification(PushClientConnection&, const WebCore::NotificationData&, RefPtr<WebCore::NotificationResources>, CompletionHandler<void()>&&);
    void getNotifications(PushClientConnection&, const URL& registrationURL, const String& tag, CompletionHandler<void(Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&&)>&&);
    void cancelNotification(PushClientConnection&, WebCore::SecurityOriginData&&, const WTF::UUID& notificationID);

    void getPushPermissionState(PushClientConnection&, const WebCore::SecurityOriginData&, CompletionHandler<void(WebCore::PushPermissionState)>&&);
    void requestPushPermission(PushClientConnection&, const WebCore::SecurityOriginData&, CompletionHandler<void(bool)>&&);
#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

    void setAppBadge(PushClientConnection&, WebCore::SecurityOriginData&&, std::optional<uint64_t>);
    void getAppBadgeForTesting(PushClientConnection&, CompletionHandler<void(std::optional<uint64_t>)>&&);

    void setProtocolVersionForTesting(PushClientConnection&, unsigned, CompletionHandler<void()>&&);

private:
    WebPushDaemon();

    void notifyClientPushMessageIsAvailable(const WebCore::PushSubscriptionSetIdentifier&);

    void setPushService(RefPtr<PushService>&&);
    void runAfterStartingPushService(Function<void()>&&);

    void handleIncomingPushImpl(const WebCore::PushSubscriptionSetIdentifier&, WebKit::WebPushMessage&&);
    void ensureIncomingPushTransaction();
    void releaseIncomingPushTransaction();
    void incomingPushTransactionTimerFired();

    Seconds silentPushTimeout() const;
    void rescheduleSilentPushTimer();
    void silentPushTimerFired();
    void didShowNotification(const WebCore::PushSubscriptionSetIdentifier&, const String& scope);

#if PLATFORM(IOS)
    void updateSubscriptionSetState();
#endif

    PushClientConnection* toPushClientConnection(xpc_connection_t);
    HashSet<xpc_connection_t> m_pendingConnectionSet;
    HashMap<xpc_connection_t, Ref<PushClientConnection>> m_connectionMap;

    RefPtr<PushService> m_pushService;
    bool m_usingMockPushService { false };
    bool m_pushServiceStarted { false };
    Deque<Function<void()>> m_pendingPushServiceFunctions;

    struct PendingPushMessage {
        WebCore::PushSubscriptionSetIdentifier identifier;
        WebKit::WebPushMessage message;
    };
    Deque<PendingPushMessage> m_pendingPushMessages;

    WebCore::Timer m_incomingPushTransactionTimer;
    OSObjectPtr<os_transaction_t> m_incomingPushTransaction;

    struct PotentialSilentPush {
        WebCore::PushSubscriptionSetIdentifier identifier;
        String scope;
        MonotonicTime expirationTime;
    };

    WebCore::Timer m_silentPushTimer;
    OSObjectPtr<os_transaction_t> m_silentPushTransaction;
    StdList<PotentialSilentPush> m_potentialSilentPushes;

#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    Class m_userNotificationCenterClass;
#endif // HAVE(FULL_FEATURED_USER_NOTIFICATIONS)

#if PLATFORM(IOS)
    RetainPtr<FBSOpenApplicationService> m_openService;
    std::unique_ptr<WebClipCache> m_webClipCache;
    String m_webClipCachePath;
#endif
};

} // namespace WebPushD

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)

