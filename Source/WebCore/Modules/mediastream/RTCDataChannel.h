/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "DetachedRTCDataChannel.h"
#include "Event.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "NetworkSendQueue.h"
#include "RTCDataChannelHandler.h"
#include "RTCDataChannelHandlerClient.h"
#include "RTCDataChannelIdentifier.h"
#include "ScriptExecutionContext.h"
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
    static Ref<RTCDataChannel> create(ScriptExecutionContext&, RTCDataChannelIdentifier, String&&, RTCDataChannelInit&&, RTCDataChannelState);

    bool ordered() const { return *m_options.ordered; }
    std::optional<unsigned short> maxPacketLifeTime() const { return m_options.maxPacketLifeTime; }
    std::optional<unsigned short> maxRetransmits() const { return m_options.maxRetransmits; }
    String protocol() const { return m_options.protocol; }
    bool negotiated() const { return *m_options.negotiated; };
    std::optional<unsigned short> id() const { return m_options.id; };
    RTCPriorityType priority() const { return m_options.priority; };
    const RTCDataChannelInit& options() const { return m_options; }

    String label() const { return m_label; }
    RTCDataChannelState readyState() const {return m_readyState; }
    size_t bufferedAmount() const { return m_bufferedAmount; }
    size_t bufferedAmountLowThreshold() const { return m_bufferedAmountLowThreshold; }
    void setBufferedAmountLowThreshold(size_t value) { m_bufferedAmountLowThreshold = value; }

    const AtomString& binaryType() const;
    ExceptionOr<void> setBinaryType(const AtomString&);

    ExceptionOr<void> send(const String&);
    ExceptionOr<void> send(JSC::ArrayBuffer&);
    ExceptionOr<void> send(JSC::ArrayBufferView&);
    ExceptionOr<void> send(Blob&);

    void close();

    RTCDataChannelIdentifier identifier() const { return m_identifier; }
    bool canDetach() const;
    std::unique_ptr<DetachedRTCDataChannel> detach();

    using RTCDataChannelHandlerClient::ref;
    using RTCDataChannelHandlerClient::deref;

    WEBCORE_EXPORT static std::unique_ptr<RTCDataChannelHandler> handlerFromIdentifier(RTCDataChannelLocalIdentifier);

private:
    RTCDataChannel(ScriptExecutionContext&, std::unique_ptr<RTCDataChannelHandler>&&, String&&, RTCDataChannelInit&&);

    static NetworkSendQueue createMessageQueue(ScriptExecutionContext&, RTCDataChannel&);

    void scheduleDispatchEvent(Ref<Event>&&);
    void removeFromDataChannelLocalMapIfNeeded();

    EventTargetInterface eventTargetInterface() const final { return RTCDataChannelEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return m_scriptExecutionContext; }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject API
    void stop() final;
    const char* activeDOMObjectName() const final { return "RTCDataChannel"; }
    bool virtualHasPendingActivity() const final;

    // RTCDataChannelHandlerClient API
    void didChangeReadyState(RTCDataChannelState) final;
    void didReceiveStringData(const String&) final;
    void didReceiveRawData(const uint8_t*, size_t) final;
    void didDetectError() final;
    void bufferedAmountIsDecreasing(size_t) final;

    std::unique_ptr<RTCDataChannelHandler> m_handler;
    RTCDataChannelIdentifier m_identifier;
    ScriptExecutionContextIdentifier m_contextIdentifier;
    // FIXME: m_stopped is probably redundant with m_readyState.
    bool m_stopped { false };
    RTCDataChannelState m_readyState { RTCDataChannelState::Connecting };

    enum class BinaryType { Blob, ArrayBuffer };
    BinaryType m_binaryType { BinaryType::ArrayBuffer };

    String m_label;
    RTCDataChannelInit m_options;
    size_t m_bufferedAmount { 0 };
    size_t m_bufferedAmountLowThreshold { 0 };
    bool m_isDetachable { true };
    bool m_isDetached { false };
    NetworkSendQueue m_messageQueue;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
