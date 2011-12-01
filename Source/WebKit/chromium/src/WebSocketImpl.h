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

#ifndef WebSocketImpl_h
#define WebSocketImpl_h

#include "WebCommon.h"
#include "WebSocket.h"
#include "WebSocketChannelClient.h"
#include "WebSocketClient.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore { class WebSocketChannel; }

namespace WebKit {

class WebData;
class WebDocument;
class WebString;
class WebURL;

class WebSocketImpl : public WebSocket, public WebCore::WebSocketChannelClient {
public:
    WebSocketImpl(const WebDocument&, WebSocketClient*);
    virtual ~WebSocketImpl();

    bool isNull() const { return !m_private; }

    virtual void connect(const WebURL&, const WebString& protocol);
    virtual WebString subprotocol();
    virtual bool sendText(const WebString& message);
    virtual bool sendBinary(const WebData& binaryData);
    virtual unsigned long bufferedAmount() const;
    virtual void close(int code, const WebString& reason);
    virtual void fail(const WebString& reason);
    virtual void disconnect();

    // WebSocketChannelClient
    virtual void didConnect();
    virtual void didReceiveMessage(const String& message);
    virtual void didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData);
    virtual void didReceiveMessageError();
    virtual void didUpdateBufferedAmount(unsigned long bufferedAmount);
    virtual void didStartClosingHandshake();
    virtual void didClose(unsigned long bufferedAmount, ClosingHandshakeCompletionStatus, unsigned short code, const String& reason);

private:
    RefPtr<WebCore::WebSocketChannel> m_private;
    WebSocketClient* m_client;
};

} // namespace WebKit

#endif // WebWebSocketChannelImpl_h
