/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "MessagePortChannel.h"
#include <wtf/MessageQueue.h>
#include <wtf/Threading.h>

namespace WebCore {

    class MessagePort;

    // PlatformMessagePortChannel is a platform-dependent interface to the remote side of a message channel.
    // This default implementation supports multiple threads running within a single process. Implementations for multi-process platforms should define these public APIs in their own platform-specific PlatformMessagePortChannel file.
    // The goal of this implementation is to eliminate contention except when cloning or closing the port, so each side of the channel has its own separate mutex.
    class PlatformMessagePortChannel : public ThreadSafeRefCounted<PlatformMessagePortChannel> {
    public:
        // Wrapper for MessageQueue that allows us to do thread safe sharing by two proxies.
        class MessagePortQueue : public ThreadSafeRefCounted<MessagePortQueue> {
        public:
            static Ref<MessagePortQueue> create() { return adoptRef(*new MessagePortQueue()); }

            std::unique_ptr<MessagePortChannel::EventData> takeMessage()
            {
                return m_queue.tryGetMessage();
            }

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

        ~PlatformMessagePortChannel();

        static Ref<PlatformMessagePortChannel> create(MessagePortQueue* incoming, MessagePortQueue* outgoing);
        PlatformMessagePortChannel(MessagePortQueue* incoming, MessagePortQueue* outgoing);

        RefPtr<PlatformMessagePortChannel> entangledChannel();

        void setRemotePort(MessagePort*);
        void closeInternal();

        // Lock used to ensure exclusive access to the object internals.
        Lock m_mutex;

        // Pointer to our entangled pair - cleared when close() is called.
        RefPtr<PlatformMessagePortChannel> m_entangledChannel;

        // Reference to the message queue for the (local) entangled port.
        RefPtr<MessagePortQueue> m_incomingQueue;
        RefPtr<MessagePortQueue> m_outgoingQueue;

        // The port we are connected to (the remote port) - this is the port that is notified when new messages arrive.
        MessagePort* m_remotePort { nullptr };
    };

} // namespace WebCore
