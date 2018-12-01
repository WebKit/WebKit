/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "Timer.h"
#include <wtf/URL.h>
#include "WebSocketChannelClient.h"
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
}

namespace WebCore {

class Blob;
class ThreadableWebSocketChannel;

class WebSocket final : public RefCounted<WebSocket>, public EventTargetWithInlineData, public ActiveDOMObject, private WebSocketChannelClient {
public:
    static const char* subprotocolSeparator();

    static ExceptionOr<Ref<WebSocket>> create(ScriptExecutionContext&, const String& url);
    static ExceptionOr<Ref<WebSocket>> create(ScriptExecutionContext&, const String& url, const String& protocol);
    static ExceptionOr<Ref<WebSocket>> create(ScriptExecutionContext&, const String& url, const Vector<String>& protocols);
    virtual ~WebSocket();

    static HashSet<WebSocket*>& allActiveWebSockets(const LockHolder&);
    static Lock& allActiveWebSocketsMutex();

    enum State {
        CONNECTING = 0,
        OPEN = 1,
        CLOSING = 2,
        CLOSED = 3
    };

    ExceptionOr<void> connect(const String& url);
    ExceptionOr<void> connect(const String& url, const String& protocol);
    ExceptionOr<void> connect(const String& url, const Vector<String>& protocols);

    ExceptionOr<void> send(const String& message);
    ExceptionOr<void> send(JSC::ArrayBuffer&);
    ExceptionOr<void> send(JSC::ArrayBufferView&);
    ExceptionOr<void> send(Blob&);

    ExceptionOr<void> close(std::optional<unsigned short> code, const String& reason);

    RefPtr<ThreadableWebSocketChannel> channel() const;

    const URL& url() const;
    State readyState() const;
    unsigned bufferedAmount() const;

    String protocol() const;
    String extensions() const;

    String binaryType() const;
    ExceptionOr<void> setBinaryType(const String&);

    ScriptExecutionContext* scriptExecutionContext() const final;

    using RefCounted::ref;
    using RefCounted::deref;

private:
    explicit WebSocket(ScriptExecutionContext&);

    void resumeTimerFired();
    void dispatchOrQueueErrorEvent();
    void dispatchOrQueueEvent(Ref<Event>&&);

    void contextDestroyed() final;
    bool canSuspendForDocumentSuspension() const final;
    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() final;
    const char* activeDOMObjectName() const final;

    EventTargetInterface eventTargetInterface() const final;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void didConnect() final;
    void didReceiveMessage(const String& message) final;
    void didReceiveBinaryData(Vector<uint8_t>&&) final;
    void didReceiveMessageError() final;
    void didUpdateBufferedAmount(unsigned bufferedAmount) final;
    void didStartClosingHandshake() final;
    void didClose(unsigned unhandledBufferedAmount, ClosingHandshakeCompletionStatus, unsigned short code, const String& reason) final;
    void didUpgradeURL() final;

    size_t getFramingOverhead(size_t payloadSize);

    enum class BinaryType { Blob, ArrayBuffer };

    RefPtr<ThreadableWebSocketChannel> m_channel;

    State m_state { CONNECTING };
    URL m_url;
    unsigned m_bufferedAmount { 0 };
    unsigned m_bufferedAmountAfterClose { 0 };
    BinaryType m_binaryType { BinaryType::Blob };
    String m_subprotocol;
    String m_extensions;

    Timer m_resumeTimer;
    bool m_shouldDelayEventFiring { false };
    Deque<Ref<Event>> m_pendingEvents;
    bool m_dispatchedErrorEvent { false };
};

} // namespace WebCore
