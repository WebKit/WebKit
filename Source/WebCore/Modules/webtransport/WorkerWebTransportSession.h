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

#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/WebTransportSession.h>
#include <WebCore/WebTransportSessionClient.h>

namespace WebCore {

class WebTransport;
class WebTransportSendStreamSink;

class WorkerWebTransportSession : public WebTransportSession, public WebTransportSessionClient {
public:
    WEBCORE_EXPORT static Ref<WorkerWebTransportSession> create(ScriptExecutionContextIdentifier, WebTransportSessionClient&);
    ~WorkerWebTransportSession();

    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

    WEBCORE_EXPORT void attachSession(Ref<WebTransportSession>&&);

private:
    WorkerWebTransportSession(ScriptExecutionContextIdentifier, WebTransportSessionClient&);

    void receiveDatagram(std::span<const uint8_t>, bool, std::optional<Exception>&&) final;
    void receiveIncomingUnidirectionalStream(WebTransportStreamIdentifier) final;
    void receiveBidirectionalStream(Ref<WebTransportSendStreamSink>&&) final;
    void streamReceiveBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool, std::optional<Exception>&&) final;
    void streamReceiveError(WebTransportStreamIdentifier, uint64_t) final;
    void streamSendError(WebTransportStreamIdentifier, uint64_t) final;
    void didFail(std::optional<uint32_t>&&, String&&) final;
    void didDrain() final;

    Ref<WebTransportSendPromise> sendDatagram(std::optional<WebTransportSendGroupIdentifier>, std::span<const uint8_t>) final;
    Ref<WritableStreamPromise> createOutgoingUnidirectionalStream() final;
    Ref<BidirectionalStreamPromise> createBidirectionalStream() final;
    Ref<WebTransportSendPromise> streamSendBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin) final;
    Ref<WebTransportConnectionStatsPromise> getStats() final;
    Ref<WebTransportSendStreamStatsPromise> getSendStreamStats(WebTransportStreamIdentifier) final;
    Ref<WebTransportReceiveStreamStatsPromise> getReceiveStreamStats(WebTransportStreamIdentifier) final;
    Ref<WebTransportSendStreamStatsPromise> getSendGroupStats(WebTransportSendGroupIdentifier) final;

    void cancelReceiveStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final;
    void cancelSendStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final;
    void destroyStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) final;
    void terminate(WebTransportSessionErrorCode, CString&&) final;
    void datagramIncomingMaxAgeUpdated(std::optional<double>) final;
    void datagramOutgoingMaxAgeUpdated(std::optional<double>) final;
    void datagramIncomingHighWaterMarkUpdated(double) final;
    void datagramOutgoingHighWaterMarkUpdated(double) final;

    const ScriptExecutionContextIdentifier m_contextID;
    ThreadSafeWeakPtr<WebTransportSessionClient> m_client;
    RefPtr<WebTransportSession> m_session;
};

}
