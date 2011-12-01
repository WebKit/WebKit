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

#ifndef ThreadableWebSocketChannelClientWrapper_h
#define ThreadableWebSocketChannelClientWrapper_h

#if ENABLE(WEB_SOCKETS)

#include "PlatformString.h"
#include "ScriptExecutionContext.h"
#include "WebSocketChannelClient.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebSocketChannelClient;

class ThreadableWebSocketChannelClientWrapper : public ThreadSafeRefCounted<ThreadableWebSocketChannelClientWrapper> {
public:
    static PassRefPtr<ThreadableWebSocketChannelClientWrapper> create(WebSocketChannelClient*);

    void clearSyncMethodDone();
    void setSyncMethodDone();
    bool syncMethodDone() const;

    // The value of useHixie76Protocol flag is cachable; this value is saved after WebSocketChannel (on the main
    // thread) is constructed.
    bool useHixie76Protocol() const;
    void setUseHixie76Protocol(bool);

    // Subprotocol is cached too. Will be available when didConnect() callback is invoked.
    String subprotocol() const;
    void setSubprotocol(const String&);

    bool sendRequestResult() const;
    void setSendRequestResult(bool);

    unsigned long bufferedAmount() const;
    void setBufferedAmount(unsigned long);

    void clearClient();

    void didConnect();
    void didReceiveMessage(const String& message);
    void didReceiveBinaryData(PassOwnPtr<Vector<char> >);
    void didUpdateBufferedAmount(unsigned long bufferedAmount);
    void didStartClosingHandshake();
    void didClose(unsigned long unhandledBufferedAmount, WebSocketChannelClient::ClosingHandshakeCompletionStatus, unsigned short code, const String& reason);

    void suspend();
    void resume();

protected:
    ThreadableWebSocketChannelClientWrapper(WebSocketChannelClient*);

    void processPendingTasks();
    static void didConnectCallback(ScriptExecutionContext*, PassRefPtr<ThreadableWebSocketChannelClientWrapper>);
    static void didReceiveMessageCallback(ScriptExecutionContext*, PassRefPtr<ThreadableWebSocketChannelClientWrapper>, const String& message);
    static void didReceiveBinaryDataCallback(ScriptExecutionContext*, PassRefPtr<ThreadableWebSocketChannelClientWrapper>, PassOwnPtr<Vector<char> >);
    static void didUpdateBufferedAmountCallback(ScriptExecutionContext*, PassRefPtr<ThreadableWebSocketChannelClientWrapper>, unsigned long bufferedAmount);
    static void didStartClosingHandshakeCallback(ScriptExecutionContext*, PassRefPtr<ThreadableWebSocketChannelClientWrapper>);
    static void didCloseCallback(ScriptExecutionContext*, PassRefPtr<ThreadableWebSocketChannelClientWrapper>, unsigned long unhandledBufferedAmount, WebSocketChannelClient::ClosingHandshakeCompletionStatus, unsigned short code, const String& reason);

    WebSocketChannelClient* m_client;
    bool m_syncMethodDone;
    bool m_useHixie76Protocol;
    Vector<UChar> m_subprotocol; // ThreadSafeRefCounted must not have a String member variable.
    bool m_sendRequestResult;
    unsigned long m_bufferedAmount;
    bool m_suspended;
    Vector<OwnPtr<ScriptExecutionContext::Task> > m_pendingTasks;
};

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)

#endif // ThreadableWebSocketChannelClientWrapper_h
