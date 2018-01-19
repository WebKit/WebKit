/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "MessagePortIdentifier.h"
#include "Process.h"
#include <wtf/HashMap.h>

namespace WebCore {

class MessagePortChannelRegistry {
public:
    ~MessagePortChannelRegistry();
    
    void didCreateMessagePortChannel(const MessagePortIdentifier& port1, const MessagePortIdentifier& port2);
    void didEntangleLocalToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote, ProcessIdentifier);
    void didDisentangleMessagePort(const MessagePortIdentifier& local);
    void didCloseMessagePort(const MessagePortIdentifier& local);
    bool didPostMessageToRemote(MessageWithMessagePorts&&, const MessagePortIdentifier& remoteTarget);
    void takeAllMessagesForPort(const MessagePortIdentifier&, Function<void(Vector<MessageWithMessagePorts>&&, Function<void()>&&)>&&);

    MessagePortChannel* existingChannelContainingPort(const MessagePortIdentifier&);
    bool hasMessagesForPorts_temporarySync(const MessagePortIdentifier&, const MessagePortIdentifier&);

    void messagePortChannelCreated(MessagePortChannel&);
    void messagePortChannelDestroyed(MessagePortChannel&);

private:

    // FIXME: The need for the open channels lock is temporary.
    // It should be removed and this class should be main-thread only.
    Lock m_openChannelsLock;
    HashMap<MessagePortIdentifier, MessagePortChannel*> m_openChannels;
};

} // namespace WebCore
