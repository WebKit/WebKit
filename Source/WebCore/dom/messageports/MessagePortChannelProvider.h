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

#include "ProcessIdentifier.h"
#include <wtf/Function.h>
#include <wtf/Vector.h>

namespace WebCore {

struct MessagePortIdentifier;
struct MessageWithMessagePorts;

class MessagePortChannelProvider {
public:
    static MessagePortChannelProvider& singleton();
    WEBCORE_EXPORT static void setSharedProvider(MessagePortChannelProvider&);

    virtual ~MessagePortChannelProvider() { }

    // Operations that WebProcesses perform
    virtual void createNewMessagePortChannel(const MessagePortIdentifier& local, const MessagePortIdentifier& remote) = 0;
    virtual void entangleLocalPortInThisProcessToRemote(const MessagePortIdentifier& local, const MessagePortIdentifier& remote) = 0;
    virtual void messagePortDisentangled(const MessagePortIdentifier& local) = 0;
    virtual void messagePortClosed(const MessagePortIdentifier& local) = 0;
    virtual void takeAllMessagesForPort(const MessagePortIdentifier&, Function<void(Vector<MessageWithMessagePorts>&&, Function<void()>&&)>&&) = 0;
    virtual void postMessageToRemote(MessageWithMessagePorts&&, const MessagePortIdentifier& remoteTarget) = 0;

    enum class HasActivity {
        Yes,
        No,
    };
    virtual void checkRemotePortForActivity(const MessagePortIdentifier& remoteTarget, CompletionHandler<void(HasActivity)>&& callback) = 0;

    // Operations that the coordinating process performs (e.g. the UIProcess)
    virtual void checkProcessLocalPortForActivity(const MessagePortIdentifier&, ProcessIdentifier, CompletionHandler<void(HasActivity)>&&) = 0;

private:

};

} // namespace WebCore
