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

#ifndef WebSocket_h
#define WebSocket_h

#if ENABLE(WEB_SOCKETS)

#include "ActiveDOMObject.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "URL.h"
#include "WebSocketChannel.h"
#include "WebSocketChannelClient.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class Blob;
class ThreadableWebSocketChannel;

class WebSocket FINAL : public RefCounted<WebSocket>, public EventTargetWithInlineData, public ActiveDOMObject, public WebSocketChannelClient {
public:
    static void setIsAvailable(bool);
    static bool isAvailable();
    static const char* subProtocolSeperator();
    static PassRefPtr<WebSocket> create(ScriptExecutionContext&);
    static PassRefPtr<WebSocket> create(ScriptExecutionContext&, const String& url, ExceptionCode&);
    static PassRefPtr<WebSocket> create(ScriptExecutionContext&, const String& url, const String& protocol, ExceptionCode&);
    static PassRefPtr<WebSocket> create(ScriptExecutionContext&, const String& url, const Vector<String>& protocols, ExceptionCode&);
    virtual ~WebSocket();

    enum State {
        CONNECTING = 0,
        OPEN = 1,
        CLOSING = 2,
        CLOSED = 3
    };

    void connect(const String& url, ExceptionCode&);
    void connect(const String& url, const String& protocol, ExceptionCode&);
    void connect(const String& url, const Vector<String>& protocols, ExceptionCode&);

    void send(const String& message, ExceptionCode&);
    void send(JSC::ArrayBuffer*, ExceptionCode&);
    void send(JSC::ArrayBufferView*, ExceptionCode&);
    void send(Blob*, ExceptionCode&);

    void close(int code, const String& reason, ExceptionCode&);
    void close(ExceptionCode& ec) { close(WebSocketChannel::CloseEventCodeNotSpecified, String(), ec); }
    void close(int code, ExceptionCode& ec) { close(code, String(), ec); }

    const URL& url() const;
    State readyState() const;
    unsigned long bufferedAmount() const;

    String protocol() const;
    String extensions() const;

    String binaryType() const;
    void setBinaryType(const String&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);

    // EventTarget functions.
    virtual EventTargetInterface eventTargetInterface() const override;
    virtual ScriptExecutionContext* scriptExecutionContext() const override;

    using RefCounted<WebSocket>::ref;
    using RefCounted<WebSocket>::deref;

    // WebSocketChannelClient functions.
    virtual void didConnect() override;
    virtual void didReceiveMessage(const String& message) override;
    virtual void didReceiveBinaryData(PassOwnPtr<Vector<char>>) override;
    virtual void didReceiveMessageError() override;
    virtual void didUpdateBufferedAmount(unsigned long bufferedAmount) override;
    virtual void didStartClosingHandshake() override;
    virtual void didClose(unsigned long unhandledBufferedAmount, ClosingHandshakeCompletionStatus, unsigned short code, const String& reason) override;

private:
    explicit WebSocket(ScriptExecutionContext&);

    // ActiveDOMObject functions.
    virtual void contextDestroyed() override;
    virtual bool canSuspend() const override;
    virtual void suspend(ReasonForSuspension) override;
    virtual void resume() override;
    virtual void stop() override;

    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

    size_t getFramingOverhead(size_t payloadSize);

    enum BinaryType {
        BinaryTypeBlob,
        BinaryTypeArrayBuffer
    };

    RefPtr<ThreadableWebSocketChannel> m_channel;

    State m_state;
    URL m_url;
    unsigned long m_bufferedAmount;
    unsigned long m_bufferedAmountAfterClose;
    BinaryType m_binaryType;
    String m_subprotocol;
    String m_extensions;
};

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)

#endif // WebSocket_h
