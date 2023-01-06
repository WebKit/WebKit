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

#include "Decoder.h"
#include "MessageReceiveQueue.h"
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/Int128.h>

namespace IPC {

enum class ReceiverName : uint8_t;

class MessageReceiveQueueMap {
public:
    MessageReceiveQueueMap() = default;
    ~MessageReceiveQueueMap() = default;

    static bool isValidMessage(const Decoder& message)
    {
        return QueueMap::isValidKey(std::make_pair(static_cast<uint8_t>(message.messageReceiverName()), message.destinationID()));
    }

    void add(MessageReceiveQueue& queue, const ReceiverMatcher& matcher) { addImpl(StoreType(&queue), matcher); }
    void add(std::unique_ptr<MessageReceiveQueue>&& queue, const ReceiverMatcher& matcher) { addImpl(StoreType(WTFMove(queue)), matcher); }
    void remove(const ReceiverMatcher&);

    MessageReceiveQueue* get(const Decoder&) const;
private:
    using StoreType = std::variant<MessageReceiveQueue*, std::unique_ptr<MessageReceiveQueue>>;
    void addImpl(StoreType&&, const ReceiverMatcher&);
    using QueueMap = HashMap<std::pair<uint8_t, UInt128>, StoreType>;
    // Key is ReceiverName. FIXME: make it possible to use ReceiverName.
    using AnyIDQueueMap = HashMap<uint8_t, StoreType>;
    QueueMap m_queues;
    AnyIDQueueMap m_anyIDQueues;
    std::optional<StoreType> m_anyReceiverQueue;
};

} // namespace IPC
