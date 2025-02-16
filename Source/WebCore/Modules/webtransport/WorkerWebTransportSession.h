/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "ScriptExecutionContextIdentifier.h"
#include "WebTransportSession.h"
#include "WebTransportSessionClient.h"

namespace WebCore {

class WebTransport;

class WorkerWebTransportSession : public WebTransportSession, public WebTransportSessionClient {
public:
    static Ref<WorkerWebTransportSession> create(ScriptExecutionContextIdentifier, WebTransportSessionClient&);
    ~WorkerWebTransportSession();

    void ref() const { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    void attachSession(Ref<WebTransportSession>&&);

private:
    WorkerWebTransportSession(ScriptExecutionContextIdentifier, WebTransportSessionClient&);

    void receiveDatagram(std::span<const uint8_t>, bool, std::optional<Exception>&&) final;
    void receiveIncomingUnidirectionalStream(WebTransportStreamIdentifier) final;
    void receiveBidirectionalStream(WebTransportBidirectionalStreamConstructionParameters&&) final;
    void streamReceiveBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool, std::optional<Exception>&&) final;
    void networkProcessCrashed() final;

    Ref<WebTransportSendPromise> sendDatagram(std::span<const uint8_t>) final;
    Ref<WritableStreamPromise> createOutgoingUnidirectionalStream() final;
    Ref<BidirectionalStreamPromise> createBidirectionalStream() final;
    void cancelReceiveStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final;
    void cancelSendStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final;
    void destroyStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final;
    void terminate(WebTransportSessionErrorCode, CString&&) final;

    const ScriptExecutionContextIdentifier m_contextID;
    ThreadSafeWeakPtr<WebTransportSessionClient> m_client;
    RefPtr<WebTransportSession> m_session;
};

}
