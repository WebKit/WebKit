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

#include "config.h"
#include "MessageReceiveQueueMap.h"

namespace IPC {

void MessageReceiveQueueMap::addImpl(StoreType&& queue, ReceiverName receiverName, uint64_t destinationID)
{
    auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
    ASSERT(!m_queues.contains(key));
    m_queues.add(key, WTFMove(queue));
}

void MessageReceiveQueueMap::remove(ReceiverName receiverName, uint64_t destinationID)
{
    auto key = std::make_pair(static_cast<uint8_t>(receiverName), destinationID);
    ASSERT(m_queues.contains(key));
    m_queues.remove(key);
}

MessageReceiveQueue* MessageReceiveQueueMap::get(const Decoder& message) const
{
    if (m_queues.isEmpty())
        return nullptr;
    auto key = std::make_pair(static_cast<uint8_t>(message.messageReceiverName()), 0);
    auto it = m_queues.find(key);
    if (it == m_queues.end()) {
        key.second = message.destinationID();
        if (key.second)
            it = m_queues.find(key);
    }
    if (it == m_queues.end())
        return nullptr;
    return WTF::visit(
        WTF::makeVisitor(
            [](const std::unique_ptr<MessageReceiveQueue>& queue) { return queue.get(); },
            [](MessageReceiveQueue* queue) { return queue; }
        ), it->value);
}

}
