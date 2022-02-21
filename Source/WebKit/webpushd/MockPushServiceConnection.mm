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

#import "config.h"
#import "MockPushServiceConnection.h"

#import <wtf/text/Base64.h>

using namespace WebCore::PushCrypto;

namespace WebPushD {

MockPushServiceConnection::MockPushServiceConnection()
{
}

MockPushServiceConnection::~MockPushServiceConnection() = default;

ClientKeys MockPushServiceConnection::generateClientKeys()
{
    // Example values from RFC8291 Section 5.
    auto publicKey = base64URLDecode("BCVxsr7N_eNgVRqvHtD0zTZsEc6-VV-JvLexhqUzORcxaOzi6-AYWXvTBHm4bjyPjs7Vd8pZGH6SRpkNtoIAiw4"_s).value();
    auto privateKey = base64URLDecode("q1dXpw3UpT5VOmu_cf_v6ih07Aems3njxI-JWgLcM94"_s).value();
    auto secret = base64URLDecode("BTBZMqHH6r4Tts7J_aSIgg"_s).value();

    return ClientKeys { P256DHKeyPair { WTFMove(publicKey), WTFMove(privateKey) }, WTFMove(secret) };
}

void MockPushServiceConnection::subscribe(const String&, const Vector<uint8_t>& vapidPublicKey, SubscribeHandler&& handler)
{
    auto alwaysRejectedKey = base64URLDecode("BEAxaUMo1s8tjORxJfnSSvWhYb4u51kg1hWT2s_9gpV7Zxar1pF_2BQ8AncuAdS2BoLhN4qaxzBy2CwHE8BBzWg"_s).value();
    if (vapidPublicKey == alwaysRejectedKey) {
        handler({ }, [NSError errorWithDomain:@"WebPush" code:-1 userInfo:nil]);
        return;
    }

    handler(@"https://webkit.org/push", nil);
}

void MockPushServiceConnection::unsubscribe(const String&, const Vector<uint8_t>&, UnsubscribeHandler&& handler)
{
    handler(true, nil);
}

Vector<String> MockPushServiceConnection::enabledTopics()
{
    return { };
}

Vector<String> MockPushServiceConnection::ignoredTopics()
{
    return { };
}

Vector<String> MockPushServiceConnection::opportunisticTopics()
{
    return { };
}

Vector<String> MockPushServiceConnection::nonWakingTopics()
{
    return { };
}

void MockPushServiceConnection::setEnabledTopics(Vector<String>&& topics)
{
}

void MockPushServiceConnection::setIgnoredTopics(Vector<String>&& topics)
{
}

void MockPushServiceConnection::setOpportunisticTopics(Vector<String>&& topics)
{
}

void MockPushServiceConnection::setNonWakingTopics(Vector<String>&& topics)
{
}

void MockPushServiceConnection::setTopicLists(TopicLists&& topicLists)
{
}

} // namespace WebPushD
