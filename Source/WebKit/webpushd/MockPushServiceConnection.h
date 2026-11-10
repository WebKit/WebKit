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

#include "PushServiceConnection.h"

namespace WebPushD {

class MockPushServiceConnection final : public PushServiceConnection {
public:
    static Ref<MockPushServiceConnection> create()
    {
        return adoptRef(*new MockPushServiceConnection());
    }

    ~MockPushServiceConnection();

    WebCore::PushCrypto::ClientKeys generateClientKeys() final;

    void subscribe(StringImpl* topic, const Vector<uint8_t>& vapidPublicKey, SubscribeHandler&&) override;
    void unsubscribe(StringImpl* topic, const Vector<uint8_t>& vapidPublicKey, UnsubscribeHandler&&) override;

    Vector<RefPtr<StringImpl>> enabledTopics() override { return m_enabledTopics; }
    Vector<RefPtr<StringImpl>> ignoredTopics() override { return m_ignoredTopics; }
    Vector<RefPtr<StringImpl>> opportunisticTopics() override { return m_opportunisticTopics; }
    Vector<RefPtr<StringImpl>> nonWakingTopics() override { return m_nonWakingTopics; }

    void setEnabledTopics(Vector<RefPtr<StringImpl>>&& enabledTopics) override { m_enabledTopics = enabledTopics; }
    void setIgnoredTopics(Vector<RefPtr<StringImpl>>&& ignoredTopics) override { m_ignoredTopics = ignoredTopics; }
    void setOpportunisticTopics(Vector<RefPtr<StringImpl>>&& opportunisticTopics) override { m_opportunisticTopics = opportunisticTopics; }
    void setNonWakingTopics(Vector<RefPtr<StringImpl>>&& nonWakingTopics) override { m_nonWakingTopics = nonWakingTopics; }

    void setTopicLists(TopicLists&&) override;

    void setPublicTokenForTesting(Vector<uint8_t>&&) override;

private:
    MockPushServiceConnection();

    Vector<RefPtr<StringImpl>> m_enabledTopics;
    Vector<RefPtr<StringImpl>> m_ignoredTopics;
    Vector<RefPtr<StringImpl>> m_opportunisticTopics;
    Vector<RefPtr<StringImpl>> m_nonWakingTopics;
};

} // namespace WebPushD
