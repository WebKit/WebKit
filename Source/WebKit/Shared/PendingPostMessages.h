/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/FrameIdentifier.h>
#include <WebCore/MessageWithMessagePorts.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/Deque.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class PendingPostMessages : public RefCounted<PendingPostMessages> {
public:
    static Ref<PendingPostMessages> create() { return adoptRef(*new PendingPostMessages()); }

    struct PendingPostMessage {
        WebCore::FrameIdentifier source;
        WebCore::SecurityOriginData sourceOrigin;
        WebCore::FrameIdentifier target;
        std::optional<WebCore::SecurityOriginData> targetOrigin;
        WebCore::MessageWithMessagePorts message;
        std::optional<WebCore::UserGestureTokenData> userGestureToken;
    };

    bool isEmpty() const { return m_pendingPostMessages.isEmpty(); }
    void append(PendingPostMessage&& message) { m_pendingPostMessages.append(WTF::move(message)); }

    Vector<PendingPostMessage> takeMessagesThroughNextPortTransfer()
    {
        Vector<PendingPostMessage> messagesToSend;
        messagesToSend.append(m_pendingPostMessages.takeFirst());
        while (!m_pendingPostMessages.isEmpty() && m_pendingPostMessages.first().message.transferredPorts.isEmpty())
            messagesToSend.append(m_pendingPostMessages.takeFirst());
        return messagesToSend;
    }

private:
    PendingPostMessages() = default;

    Deque<PendingPostMessage> m_pendingPostMessages;
};

}
