/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "MessageReceiveQueue.h"

namespace IPC {

class FunctionDispatcherQueue final : public MessageReceiveQueue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FunctionDispatcherQueue(FunctionDispatcher& dispatcher, MessageReceiver& receiver)
        : m_dispatcher(dispatcher)
        , m_receiver(receiver)
    {
    }
    ~FunctionDispatcherQueue() final = default;

    void enqueueMessage(Connection& connection, std::unique_ptr<Decoder>&& message) final
    {
        m_dispatcher.dispatch([connection = makeRef(connection), message = WTFMove(message), &receiver = m_receiver]() mutable {
            connection->dispatchMessageReceiverMessage(receiver, WTFMove(message));
        });
    }
private:
    FunctionDispatcher& m_dispatcher;
    MessageReceiver& m_receiver;
};

class ThreadMessageReceiverQueue final : public MessageReceiveQueue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadMessageReceiverQueue(Connection::ThreadMessageReceiver& receiver)
        : m_receiver(makeRef(receiver))
    {
    }
    ~ThreadMessageReceiverQueue() final = default;

    void enqueueMessage(Connection& connection, std::unique_ptr<Decoder>&& message) final
    {
        m_receiver->dispatchToThread([connection = makeRef(connection), message = WTFMove(message), receiver = m_receiver]() mutable {
            connection->dispatchMessageReceiverMessage(receiver.get(), WTFMove(message));
        });
    }
private:
    Ref<Connection::ThreadMessageReceiver> m_receiver;
};

class WorkQueueMessageReceiverQueue final : public MessageReceiveQueue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WorkQueueMessageReceiverQueue(WorkQueue& queue, Connection::WorkQueueMessageReceiver& receiver)
        : m_queue(makeRef(queue))
        , m_receiver(makeRef(receiver))
    {
    }
    ~WorkQueueMessageReceiverQueue() final = default;

    void enqueueMessage(Connection& connection, std::unique_ptr<Decoder>&& message) final
    {
        m_queue->dispatch([connection = makeRef(connection), message = WTFMove(message), receiver = m_receiver]() mutable {
            connection->dispatchMessageReceiverMessage(receiver.get(), WTFMove(message));
        });
    }
private:
    Ref<WorkQueue> m_queue;
    Ref<Connection::WorkQueueMessageReceiver> m_receiver;
};

} // namespace IPC
