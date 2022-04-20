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

#include <WebCore/PushMessageCrypto.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSError;
OBJC_CLASS NSString;
OBJC_CLASS NSDictionary;

namespace WebPushD {

class PushServiceConnection : public CanMakeWeakPtr<PushServiceConnection> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using IncomingPushMessageHandler = Function<void(NSString *, NSDictionary *)>;

    PushServiceConnection() = default;
    virtual ~PushServiceConnection() = default;

    virtual WebCore::PushCrypto::ClientKeys generateClientKeys();

    using SubscribeHandler = CompletionHandler<void(NSString *, NSError *)>;
    virtual void subscribe(const String& topic, const Vector<uint8_t>& vapidPublicKey, SubscribeHandler&&) = 0;

    using UnsubscribeHandler = CompletionHandler<void(bool, NSError *)>;
    virtual void unsubscribe(const String& topic, const Vector<uint8_t>& vapidPublicKey, UnsubscribeHandler&&) = 0;

    virtual Vector<String> enabledTopics() = 0;
    virtual Vector<String> ignoredTopics() = 0;
    virtual Vector<String> opportunisticTopics() = 0;
    virtual Vector<String> nonWakingTopics() = 0;

    virtual void setEnabledTopics(Vector<String>&&) = 0;
    virtual void setIgnoredTopics(Vector<String>&&) = 0;
    virtual void setOpportunisticTopics(Vector<String>&&) = 0;
    virtual void setNonWakingTopics(Vector<String>&&) = 0;

    struct TopicLists {
        Vector<String> enabledTopics;
        Vector<String> ignoredTopics;
        Vector<String> opportunisticTopics;
        Vector<String> nonWakingTopics;
    };
    virtual void setTopicLists(TopicLists&&) = 0;

    void startListeningForPublicToken(Function<void(Vector<uint8_t>&&)>&&);
    void didReceivePublicToken(Vector<uint8_t>&&);
    virtual void setPublicTokenForTesting(Vector<uint8_t>&&);

    void startListeningForPushMessages(IncomingPushMessageHandler&&);
    void didReceivePushMessage(NSString *topic, NSDictionary *userInfo);

private:
    Function<void(Vector<uint8_t>&&)> m_publicTokenChangeHandler;
    Vector<uint8_t> m_pendingPublicToken;
    IncomingPushMessageHandler m_incomingPushMessageHandler;
    Deque<std::pair<RetainPtr<NSString>, RetainPtr<NSDictionary>>> m_pendingPushes;
};

} // namespace WebPushD
