/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/ProcessQualified.h>
#include <WebCore/WebTransportSession.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

class WebTransportBidirectionalStream;
class WebTransportReceiveStream;
class WebTransportReceiveStreamSource;
class WebTransportSendStream;

struct WebTransportStreamIdentifierType { };
struct WebTransportSessionIdentifierType { };

using WebTransportStreamIdentifier = LegacyNullableObjectIdentifier<WebTransportStreamIdentifierType>;
using WebTransportSessionIdentifier = LegacyNullableObjectIdentifier<WebTransportSessionIdentifierType>;

class WebTransportSession : public WebCore::WebTransportSession, public IPC::MessageReceiver, public IPC::MessageSender, public ThreadSafeRefCounted<WebTransportSession, WTF::DestructionThread::MainRunLoop> {
public:
    static void initialize(const URL&, CompletionHandler<void(RefPtr<WebTransportSession>&&)>&&);
    ~WebTransportSession();

    void receiveDatagram(std::span<const uint8_t>);
    void receiveIncomingUnidirectionalStream(WebTransportStreamIdentifier);
    void receiveBidirectionalStream(WebTransportStreamIdentifier);
    void streamReceiveBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin);

    void streamSendBytes(WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin, CompletionHandler<void()>&&);

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    // MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
private:
    WebTransportSession(WebTransportSessionIdentifier);

    // WebTransportSession
    void sendDatagram(std::span<const uint8_t>, CompletionHandler<void()>&&) final;
    void createOutgoingUnidirectionalStream(CompletionHandler<void(RefPtr<WebCore::WritableStreamSink>&&)>&&) final;
    void createBidirectionalStream(CompletionHandler<void(std::optional<WebCore::WebTransportBidirectionalStreamConstructionParameters>&&)>&&) final;
    void terminate(uint32_t, CString&&) final;

    // MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    WebTransportSessionIdentifier m_identifier;
    HashMap<WebTransportStreamIdentifier, Ref<WebTransportReceiveStreamSource>> m_readStreamSources;
};

}
