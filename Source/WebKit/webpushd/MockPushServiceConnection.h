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
    MockPushServiceConnection();
    ~MockPushServiceConnection();

    WebCore::PushCrypto::ClientKeys generateClientKeys() final;

    void subscribe(const String& topic, const Vector<uint8_t>& vapidPublicKey, SubscribeHandler&&) override;
    void unsubscribe(const String& topic, const Vector<uint8_t>& vapidPublicKey, UnsubscribeHandler&&) override;

    Vector<String> enabledTopics() override { return m_enabledTopics; }
    Vector<String> ignoredTopics() override { return m_ignoredTopics; }
    Vector<String> opportunisticTopics() override { return m_opportunisticTopics; }
    Vector<String> nonWakingTopics() override { return m_nonWakingTopics; }

    void setEnabledTopics(Vector<String>&& enabledTopics) override { m_enabledTopics = enabledTopics; }
    void setIgnoredTopics(Vector<String>&& ignoredTopics) override { m_ignoredTopics = ignoredTopics; }
    void setOpportunisticTopics(Vector<String>&& opportunisticTopics) override { m_opportunisticTopics = opportunisticTopics; }
    void setNonWakingTopics(Vector<String>&& nonWakingTopics) override { m_nonWakingTopics = nonWakingTopics; }

    void setTopicLists(TopicLists&&) override;

    void setPublicTokenForTesting(Vector<uint8_t>&&) override;

private:
    Vector<String> m_enabledTopics;
    Vector<String> m_ignoredTopics;
    Vector<String> m_opportunisticTopics;
    Vector<String> m_nonWakingTopics;
};

} // namespace WebPushD
