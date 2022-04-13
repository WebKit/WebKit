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
#include <wtf/text/TextStream.h>

namespace IPC {

void MessageReceiveQueueMap::addImpl(StoreType&& queue, const ReceiverMatcher& matcher)
{
    if (!matcher.receiverName) {
        ASSERT(!m_anyReceiverQueue);
        m_anyReceiverQueue = WTFMove(queue);
        return;
    }
    uint8_t receiverName = static_cast<uint8_t>(*matcher.receiverName);
    if (!matcher.destinationID.has_value()) {
        auto result = m_anyIDQueues.add(receiverName, WTFMove(queue));
        ASSERT_UNUSED(result, result.isNewEntry);
        return;
    }
    auto result = m_queues.add(std::make_pair(receiverName, *matcher.destinationID), WTFMove(queue));
    ASSERT_UNUSED(result, result.isNewEntry);
}

void MessageReceiveQueueMap::remove(const ReceiverMatcher& matcher)
{
    if (!matcher.receiverName) {
        ASSERT(m_anyReceiverQueue);
        m_anyReceiverQueue = nullptr;
        return;
    }
    uint8_t receiverName = static_cast<uint8_t>(*matcher.receiverName);
    if (!matcher.destinationID.has_value()) {
        auto result = m_anyIDQueues.remove(receiverName);
        ASSERT_UNUSED(result, result);
        return;
    }
    auto result = m_queues.remove(std::make_pair(receiverName, *matcher.destinationID));
    ASSERT_UNUSED(result, result);
}

MessageReceiveQueue* MessageReceiveQueueMap::get(const Decoder& message) const
{
    uint8_t receiverName = static_cast<uint8_t>(message.messageReceiverName());
    auto queueExtractor = WTF::makeVisitor(
        [](const std::unique_ptr<MessageReceiveQueue>& queue) { return queue.get(); },
        [](MessageReceiveQueue* queue) { return queue; }
    );

    {
        auto it = m_anyIDQueues.find(receiverName);
        if (it != m_anyIDQueues.end())
            return std::visit(queueExtractor, it->value);
    }
    {
        auto it = m_queues.find(std::make_pair(receiverName, message.destinationID()));
        if (it != m_queues.end())
            return std::visit(queueExtractor, it->value);
    }
    if (m_anyReceiverQueue)
        return std::visit(queueExtractor, *m_anyReceiverQueue);
    return nullptr;
}

}
