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
#include "WebPageProxyIdentifier.h"
#include <WebCore/ProcessQualified.h>
#include <WebCore/WebTransportSession.h>
#include <wtf/NativePromise.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace IPC {
enum class Error : uint8_t;
}

namespace WebCore {
class WebTransportSession;
struct ClientOrigin;
struct WebTransportStreamIdentifierType;
using WebTransportStreamIdentifier = ObjectIdentifier<WebTransportStreamIdentifierType>;
using WebTransportSessionPromise = NativePromise<Ref<WebTransportSession>, void>;
}

namespace WebKit {

class WebTransportBidirectionalStream;
class WebTransportReceiveStream;
class WebTransportSendStream;

struct WebTransportSessionIdentifierType { };

using WebTransportSessionIdentifier = ObjectIdentifier<WebTransportSessionIdentifierType>;

class WebTransportSession : public WebCore::WebTransportSession, public IPC::MessageReceiver, public IPC::MessageSender, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebTransportSession, WTF::DestructionThread::MainRunLoop> {
public:
    static Ref<WebCore::WebTransportSessionPromise> initialize(Ref<IPC::Connection>&&, const URL&, const WebPageProxyIdentifier&, const WebCore::ClientOrigin&);
    ~WebTransportSession();

    void receiveDatagram(std::span<const uint8_t>);
    void receiveIncomingUnidirectionalStream(WebCore::WebTransportStreamIdentifier);
    void receiveBidirectionalStream(WebCore::WebTransportStreamIdentifier);
    void streamReceiveBytes(WebCore::WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin);

    Ref<GenericPromise> streamSendBytes(WebCore::WebTransportStreamIdentifier, std::span<const uint8_t>, bool withFin);

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    // MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void networkProcessCrashed();
private:
    WebTransportSession(Ref<IPC::Connection>&&, WebTransportSessionIdentifier);

    // WebTransportSession
    Ref<GenericPromise> sendDatagram(std::span<const uint8_t>) final;
    Ref<WebCore::WritableStreamPromise> createOutgoingUnidirectionalStream() final;
    Ref<WebCore::BidirectionalStreamPromise> createBidirectionalStream() final;
    void terminate(uint32_t, CString&&) final;

    // MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    const Ref<IPC::Connection> m_connection;
    const WebTransportSessionIdentifier m_identifier;
};

}
