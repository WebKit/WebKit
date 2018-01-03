/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "MessagePortChannel.h"
#include <wtf/MessageQueue.h>

namespace WebCore {

class InProcessMessagePortChannel : public MessagePortChannel {
public:
    static void createChannelBetweenPorts(MessagePort&, MessagePort&);

    ~InProcessMessagePortChannel() final;

    void postMessageToRemote(Ref<SerializedScriptValue>&&, std::unique_ptr<MessagePortChannelArray>&&) final;
    void takeAllMessagesFromRemote(CompletionHandler<void(Deque<std::unique_ptr<EventData>>&&)>&&) final;
    bool isConnectedTo(const MessagePortIdentifier&) final;
    bool entangleWithRemoteIfOpen(const MessagePortIdentifier&) final;
    void disentangle() final;
    bool hasPendingActivity() final;
    MessagePort* locallyEntangledPort(const ScriptExecutionContext*) final;
    void close() final;

private:
    // Wrapper for MessageQueue that allows us to do thread safe sharing by two proxies.
    class MessagePortQueue : public ThreadSafeRefCounted<MessagePortQueue> {
    public:
        static Ref<MessagePortQueue> create() { return adoptRef(*new MessagePortQueue()); }

        Deque<std::unique_ptr<MessagePortChannel::EventData>> takeAllMessages()
        {
            return m_queue.takeAllMessages();
        }

        bool appendAndCheckEmpty(std::unique_ptr<MessagePortChannel::EventData>&& message)
        {
            return m_queue.appendAndCheckEmpty(WTFMove(message));
        }

        bool isEmpty()
        {
            return m_queue.isEmpty();
        }

    private:
        MessagePortQueue() { }

        MessageQueue<MessagePortChannel::EventData> m_queue;
    };

    static Ref<InProcessMessagePortChannel> create(MessagePortQueue& incoming, MessagePortQueue& outgoing);
    InProcessMessagePortChannel(MessagePortQueue& incoming, MessagePortQueue& outgoing);

    void setRemotePort(MessagePort*);
    RefPtr<InProcessMessagePortChannel> takeEntangledChannel();

    Lock m_lock;

    RefPtr<InProcessMessagePortChannel> m_entangledChannel;

    RefPtr<MessagePortQueue> m_incomingQueue;
    RefPtr<MessagePortQueue> m_outgoingQueue;

    MessagePort* m_remotePort { nullptr };
};

} // namespace WebCore
