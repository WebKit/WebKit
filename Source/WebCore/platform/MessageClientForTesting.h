/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/Ref.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

struct MessageForTesting;

class MessageClientForTesting : public AbstractRefCountedAndCanMakeWeakPtr<MessageClientForTesting> {
public:
    virtual void sendInternalMessage(const MessageForTesting&) = 0;
};

class AggregateMessageClientForTesting
    : public MessageClientForTesting
    , public RefCounted<AggregateMessageClientForTesting> {
public:
    static Ref<AggregateMessageClientForTesting> create() { return *new AggregateMessageClientForTesting(); }

    void addClient(MessageClientForTesting& client) { m_clients.add(client); }
    void removeClient(const MessageClientForTesting& client) { m_clients.remove(client); }
    bool isEmpty() const { return m_clients.isEmptyIgnoringNullReferences(); }

    void sendInternalMessage(const MessageForTesting& message) override
    {
        m_clients.forEach([&] (auto& client) {
            Ref { client }->sendInternalMessage(message);
        });
    }

    void ref() const override { return RefCounted::ref(); }
    void deref() const override { return RefCounted::deref(); }

private:
    AggregateMessageClientForTesting() = default;

    WeakHashSet<MessageClientForTesting> m_clients;
};

}
