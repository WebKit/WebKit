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

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "WebTransportOptions.h"
#include "WebTransportReliabilityMode.h"
#include "WebTransportSessionClient.h"
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

enum class WebTransportCongestionControl : uint8_t;

class DOMException;
class DOMPromise;
class DatagramSource;
class DeferredPromise;
class JSDOMGlobalObject;
class ReadableStream;
class ScriptExecutionContext;
class SocketProvider;
class WebTransportBidirectionalStreamSource;
class WebTransportDatagramDuplexStream;
class WebTransportError;
class WebTransportReceiveStreamSource;
class WebTransportSession;
class WritableStream;

struct WebTransportCloseInfo;
struct WebTransportSendStreamOptions;
struct WebTransportHash;

class WebTransport : public WebTransportSessionClient, public ActiveDOMObject {
public:
    static ExceptionOr<Ref<WebTransport>> create(ScriptExecutionContext&, String&&, WebTransportOptions&&);
    ~WebTransport();

    void getStats(Ref<DeferredPromise>&&);
    DOMPromise& ready();
    WebTransportReliabilityMode reliability();
    WebTransportCongestionControl congestionControl();
    DOMPromise& closed();
    DOMPromise& draining();
    void close(WebTransportCloseInfo&&);
    WebTransportDatagramDuplexStream& datagrams();
    void createBidirectionalStream(ScriptExecutionContext&, WebTransportSendStreamOptions&&, Ref<DeferredPromise>&&);
    ReadableStream& incomingBidirectionalStreams();
    void createUnidirectionalStream(ScriptExecutionContext&, WebTransportSendStreamOptions&&, Ref<DeferredPromise>&&);
    ReadableStream& incomingUnidirectionalStreams();

    RefPtr<WebTransportSession> session();

private:
    WebTransport(ScriptExecutionContext&, JSDOMGlobalObject&, Ref<ReadableStream>&&, Ref<ReadableStream>&&, WebTransportCongestionControl, Ref<WebTransportDatagramDuplexStream>&&, Ref<DatagramSource>&&, Ref<WebTransportReceiveStreamSource>&&, Ref<WebTransportBidirectionalStreamSource>&&);

    void initializeOverHTTP(SocketProvider&, ScriptExecutionContext&, URL&&, bool dedicated, bool http3Only, WebTransportCongestionControl, Vector<WebTransportHash>&&);
    void cleanup(Ref<DOMException>&&, std::optional<WebTransportCloseInfo>&&);

    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    void receiveDatagram(std::span<const uint8_t>) final;
    void receiveIncomingUnidirectionalStream() final;
    void receiveBidirectionalStream() final;

    ListHashSet<Ref<WritableStream>> m_sendStreams;
    ListHashSet<Ref<ReadableStream>> m_receiveStreams;
    Ref<ReadableStream> m_incomingBidirectionalStreams;
    Ref<ReadableStream> m_incomingUnidirectionalStreams;

    // https://www.w3.org/TR/webtransport/#dom-webtransport-state-slot
    enum class State : uint8_t {
        Connecting,
        Connected,
        Draining,
        Closed,
        Failed
    };
    State m_state { State::Connecting };

    using PromiseAndWrapper = std::pair<Ref<DOMPromise>, Ref<DeferredPromise>>;
    PromiseAndWrapper m_ready;
    WebTransportReliabilityMode m_reliability { WebTransportReliabilityMode::Pending };
    WebTransportCongestionControl m_congestionControl;
    PromiseAndWrapper m_closed;
    PromiseAndWrapper m_draining;
    Ref<WebTransportDatagramDuplexStream> m_datagrams;
    RefPtr<WebTransportSession> m_session;

    Ref<DatagramSource> m_datagramSource;
    Ref<WebTransportReceiveStreamSource> m_receiveStreamSource;
    Ref<WebTransportBidirectionalStreamSource> m_bidirectionalStreamSource;
};

}
