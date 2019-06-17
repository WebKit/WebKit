/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "ActiveDOMObject.h"
#include "Event.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "RTCDataChannelHandler.h"
#include "RTCDataChannelHandlerClient.h"
#include "ScriptWrappable.h"
#include "Timer.h"

namespace JSC {
    class ArrayBuffer;
    class ArrayBufferView;
}

namespace WebCore {

class Blob;
class RTCPeerConnectionHandler;

class RTCDataChannel final : public ActiveDOMObject, public RTCDataChannelHandlerClient, public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED(RTCDataChannel);
public:
    static Ref<RTCDataChannel> create(ScriptExecutionContext&, std::unique_ptr<RTCDataChannelHandler>&&, String&&, RTCDataChannelInit&&);

    bool ordered() const { return *m_options.ordered; }
    Optional<unsigned short> maxPacketLifeTime() const { return m_options.maxPacketLifeTime; }
    Optional<unsigned short> maxRetransmits() const { return m_options.maxRetransmits; }
    String protocol() const { return m_options.protocol; }
    bool negotiated() const { return *m_options.negotiated; };
    Optional<unsigned short> id() const { return m_options.id; };

    String label() const { return m_label; }
    RTCDataChannelState readyState() const {return m_readyState; }
    size_t bufferedAmount() const;
    size_t bufferedAmountLowThreshold() const { return m_bufferedAmountLowThreshold; }
    void setBufferedAmountLowThreshold(size_t value) { m_bufferedAmountLowThreshold = value; }

    const AtomString& binaryType() const;
    ExceptionOr<void> setBinaryType(const AtomString&);

    ExceptionOr<void> send(const String&);
    ExceptionOr<void> send(JSC::ArrayBuffer&);
    ExceptionOr<void> send(JSC::ArrayBufferView&);
    ExceptionOr<void> send(Blob&);

    void close();

    using RTCDataChannelHandlerClient::ref;
    using RTCDataChannelHandlerClient::deref;

private:
    RTCDataChannel(ScriptExecutionContext&, std::unique_ptr<RTCDataChannelHandler>&&, String&&, RTCDataChannelInit&&);

    void scheduleDispatchEvent(Ref<Event>&&);
    void scheduledEventTimerFired();

    EventTargetInterface eventTargetInterface() const final { return RTCDataChannelEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return m_scriptExecutionContext; }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    ExceptionOr<void> sendRawData(const char* data, size_t length);

    // ActiveDOMObject API
    void stop() final;
    const char* activeDOMObjectName() const final { return "RTCDataChannel"; }
    bool canSuspendForDocumentSuspension() const final { return m_readyState == RTCDataChannelState::Closed; }

    // RTCDataChannelHandlerClient API
    void didChangeReadyState(RTCDataChannelState) final;
    void didReceiveStringData(const String&) final;
    void didReceiveRawData(const char*, size_t) final;
    void didDetectError() final;
    void bufferedAmountIsDecreasing(size_t) final;

    std::unique_ptr<RTCDataChannelHandler> m_handler;

    // FIXME: m_stopped is probably redundant with m_readyState.
    bool m_stopped { false };
    RTCDataChannelState m_readyState { RTCDataChannelState::Connecting };

    enum class BinaryType { Blob, ArrayBuffer };
    BinaryType m_binaryType { BinaryType::ArrayBuffer };

    Timer m_scheduledEventTimer;
    Vector<Ref<Event>> m_scheduledEvents;

    String m_label;
    RTCDataChannelInit m_options;
    size_t m_bufferedAmountLowThreshold { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
