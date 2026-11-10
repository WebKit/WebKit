/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

#include "ApplePushServiceSPI.h"
#include "PushServiceConnection.h"
#include <wtf/HashMap.h>

namespace WebPushD {

class ApplePushServiceConnection final : public PushServiceConnection {
public:
    static Ref<ApplePushServiceConnection> create(StringImpl* incomingPushServiceName)
    {
        return adoptRef(*new ApplePushServiceConnection(incomingPushServiceName));
    }

    ~ApplePushServiceConnection();

    void subscribe(StringImpl* topic, const Vector<uint8_t>& vapidPublicKey, SubscribeHandler&&) final;
    void unsubscribe(StringImpl* topic, const Vector<uint8_t>& vapidPublicKey, UnsubscribeHandler&&) final;

    Vector<RefPtr<StringImpl>> enabledTopics() override;
    Vector<RefPtr<StringImpl>> ignoredTopics() override;
    Vector<RefPtr<StringImpl>> opportunisticTopics() override;
    Vector<RefPtr<StringImpl>> nonWakingTopics() override;

    void setEnabledTopics(Vector<RefPtr<StringImpl>>&&) override;
    void setIgnoredTopics(Vector<RefPtr<StringImpl>>&&) override;
    void setOpportunisticTopics(Vector<RefPtr<StringImpl>>&&) override;
    void setNonWakingTopics(Vector<RefPtr<StringImpl>>&&) override;

    void setTopicLists(TopicLists&&) override;

private:
    ApplePushServiceConnection(StringImpl* incomingPushServiceName);

    const RetainPtr<APSConnection> m_connection;
    const RetainPtr<id<APSConnectionDelegate>> m_delegate;
    unsigned m_handlerIdentifier { 0 };
    HashMap<unsigned, SubscribeHandler> m_subscribeHandlers;
    HashMap<unsigned, UnsubscribeHandler> m_unsubscribeHandlers;
};

} // namespace WebPushD

#endif // HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

