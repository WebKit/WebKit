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
#import "PushServiceConnection.h"

#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>

namespace WebPushD {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PushServiceConnection);

PushCrypto::ClientKeys PushServiceConnection::generateClientKeys()
{
    return PushCrypto::ClientKeys::generate();
}

void PushServiceConnection::startListeningForPublicToken(Function<void(Vector<uint8_t>&&)>&& handler)
{
    m_publicTokenChangeHandler = WTFMove(handler);

    if (m_pendingPublicToken.isEmpty())
        return;

    m_publicTokenChangeHandler(std::exchange(m_pendingPublicToken, { }));
}

void PushServiceConnection::didReceivePublicToken(Vector<uint8_t>&& token)
{
    if (!m_publicTokenChangeHandler) {
        m_pendingPublicToken = WTFMove(token);
        return;
    }

    m_publicTokenChangeHandler(WTFMove(token));
}

void PushServiceConnection::setPublicTokenForTesting(Vector<uint8_t>&&)
{
}

void PushServiceConnection::startListeningForPushMessages(IncomingPushMessageHandler&& handler)
{
    m_incomingPushMessageHandler = WTFMove(handler);

    if (!m_pendingPushes.size())
        return;

    WorkQueue::mainSingleton().dispatch([this, protectdThis = Ref { *this }]() mutable {
        while (m_pendingPushes.size()) {
            @autoreleasepool {
                auto message = m_pendingPushes.takeFirst();
                m_incomingPushMessageHandler(message.first.get(), message.second.get());
            }
        }
    });
}

void PushServiceConnection::didReceivePushMessage(NSString *topic, NSDictionary *userInfo)
{
    if (!m_incomingPushMessageHandler) {
        m_pendingPushes.append({ topic, userInfo });
        return;
    }

    m_incomingPushMessageHandler(topic, userInfo);
}

} // namespace WebPushD
