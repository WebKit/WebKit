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

#include "PushServiceConnection.h"
#include "WebPushMessage.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/PushDatabase.h>
#include <WebCore/PushSubscriptionData.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebPushD {

class GetSubscriptionRequest;
class PushServiceRequest;
class SubscribeRequest;
class UnsubscribeRequest;

class PushService {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using IncomingPushMessageHandler = Function<void(const WebCore::PushSubscriptionSetIdentifier&, WebKit::WebPushMessage&&)>;

    static void create(const String& incomingPushServiceName, const String& databasePath, IncomingPushMessageHandler&&, CompletionHandler<void(std::unique_ptr<PushService>&&)>&&);
    static void createMockService(IncomingPushMessageHandler&&, CompletionHandler<void(std::unique_ptr<PushService>&&)>&&);
    ~PushService();

    PushServiceConnection& connection() { return m_connection; }
    WebCore::PushDatabase& database() { return m_database; }

    Vector<String> enabledTopics() { return m_connection->enabledTopics(); }
    Vector<String> ignoredTopics() { return m_connection->ignoredTopics(); }

    void getSubscription(const WebCore::PushSubscriptionSetIdentifier&, const String& scope, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&&);
    void subscribe(const WebCore::PushSubscriptionSetIdentifier&, const String& scope, const Vector<uint8_t>& vapidPublicKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&&);
    void unsubscribe(const WebCore::PushSubscriptionSetIdentifier&, const String& scope, std::optional<WebCore::PushSubscriptionIdentifier>, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&&);

    void incrementSilentPushCount(const WebCore::PushSubscriptionSetIdentifier&, const String& securityOrigin, CompletionHandler<void(unsigned)>&&);

    void setPushesEnabledForSubscriptionSetAndOrigin(const WebCore::PushSubscriptionSetIdentifier&, const String& securityOrigin, bool, CompletionHandler<void()>&&);

    void removeRecordsForSubscriptionSet(const WebCore::PushSubscriptionSetIdentifier&, CompletionHandler<void(unsigned)>&&);
    void removeRecordsForSubscriptionSetAndOrigin(const WebCore::PushSubscriptionSetIdentifier&, const String& securityOrigin, CompletionHandler<void(unsigned)>&&);

    void didCompleteGetSubscriptionRequest(GetSubscriptionRequest&);
    void didCompleteSubscribeRequest(SubscribeRequest&);
    void didCompleteUnsubscribeRequest(UnsubscribeRequest&);

    void setPublicTokenForTesting(Vector<uint8_t>&&);
    void didReceivePublicToken(Vector<uint8_t>&&);
    void didReceivePushMessage(NSString *topic, NSDictionary *userInfo, CompletionHandler<void()>&& = [] { });

private:
    PushService(UniqueRef<PushServiceConnection>&&, UniqueRef<WebCore::PushDatabase>&&, IncomingPushMessageHandler&&);

    using PushServiceRequestMap = HashMap<String, Deque<std::unique_ptr<PushServiceRequest>>>;
    void enqueuePushServiceRequest(PushServiceRequestMap&, std::unique_ptr<PushServiceRequest>&&);
    void finishedPushServiceRequest(PushServiceRequestMap&, PushServiceRequest&);

    void removeRecordsImpl(const WebCore::PushSubscriptionSetIdentifier&, const std::optional<String>& securityOrigin, CompletionHandler<void(unsigned)>&&);

    UniqueRef<PushServiceConnection> m_connection;
    UniqueRef<WebCore::PushDatabase> m_database;

    IncomingPushMessageHandler m_incomingPushMessageHandler;

    PushServiceRequestMap m_getSubscriptionRequests;
    PushServiceRequestMap m_subscribeRequests;
    PushServiceRequestMap m_unsubscribeRequests;
};

} // namespace WebPushD
