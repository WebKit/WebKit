/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef SocketStreamHandleInternal_h
#define SocketStreamHandleInternal_h

#if ENABLE(WEB_SOCKETS)

#include "SocketStreamHandle.h"
#include <public/WebSocketStreamHandleClient.h>
#include <public/WebURL.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class WebData;
class WebSocketStreamError;
class WebSocketStreamHandle;

}

namespace WebCore {

class SocketStreamHandleInternal : public WebKit::WebSocketStreamHandleClient {
public:
    static PassOwnPtr<SocketStreamHandleInternal> create(SocketStreamHandle* handle)
    {
        return adoptPtr(new SocketStreamHandleInternal(handle));
    }
    virtual ~SocketStreamHandleInternal();

    void connect(const KURL&);
    int send(const char*, int);
    void close();

    virtual void didOpenStream(WebKit::WebSocketStreamHandle*, int);
    virtual void didSendData(WebKit::WebSocketStreamHandle*, int);
    virtual void didReceiveData(WebKit::WebSocketStreamHandle*, const WebKit::WebData&);
    virtual void didClose(WebKit::WebSocketStreamHandle*);
    virtual void didFail(WebKit::WebSocketStreamHandle*, const WebKit::WebSocketStreamError&);

    static WebKit::WebSocketStreamHandle* toWebSocketStreamHandle(SocketStreamHandle* handle)
    {
        if (handle && handle->m_internal)
            return handle->m_internal->m_socket.get();
        return 0;
    }

private:
    explicit SocketStreamHandleInternal(SocketStreamHandle*);

    SocketStreamHandle* m_handle;
    OwnPtr<WebKit::WebSocketStreamHandle> m_socket;
    int m_maxPendingSendAllowed;
    int m_pendingAmountSent;
};

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)

#endif // SocketStreamHandleInternal_h
