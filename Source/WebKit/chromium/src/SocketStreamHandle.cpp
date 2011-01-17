/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "SocketStreamHandle.h"

#if ENABLE(WEB_SOCKETS)

#include "Logging.h"
#include "NotImplemented.h"
#include "SocketStreamHandleClient.h"
#include "WebData.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebSocketStreamHandle.h"
#include "WebSocketStreamHandleClient.h"
#include "WebURL.h"
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace WebCore {

class SocketStreamHandleInternal : public WebSocketStreamHandleClient {
public:
    static PassOwnPtr<SocketStreamHandleInternal> create(SocketStreamHandle* handle)
    {
        return new SocketStreamHandleInternal(handle);
    }
    virtual ~SocketStreamHandleInternal();

    void connect(const KURL&);
    int send(const char*, int);
    void close();

    virtual void didOpenStream(WebSocketStreamHandle*, int);
    virtual void didSendData(WebSocketStreamHandle*, int);
    virtual void didReceiveData(WebSocketStreamHandle*, const WebData&);
    virtual void didClose(WebSocketStreamHandle*);
    virtual void didFail(WebSocketStreamHandle*, const WebSocketStreamError&);

private:
    explicit SocketStreamHandleInternal(SocketStreamHandle*);

    SocketStreamHandle* m_handle;
    OwnPtr<WebSocketStreamHandle> m_socket;
    int m_maxPendingSendAllowed;
    int m_pendingAmountSent;
};

SocketStreamHandleInternal::SocketStreamHandleInternal(SocketStreamHandle* handle)
    : m_handle(handle)
    , m_maxPendingSendAllowed(0)
    , m_pendingAmountSent(0)
{
}

SocketStreamHandleInternal::~SocketStreamHandleInternal()
{
    m_handle = 0;
}

void SocketStreamHandleInternal::connect(const KURL& url)
{
    m_socket.set(webKitClient()->createSocketStreamHandle());
    LOG(Network, "connect");
    ASSERT(m_socket.get());
    m_socket->connect(url, this);
}

int SocketStreamHandleInternal::send(const char* data, int len)
{
    LOG(Network, "send len=%d", len);
    ASSERT(m_socket.get());
    if (m_pendingAmountSent + len >= m_maxPendingSendAllowed)
        len = m_maxPendingSendAllowed - m_pendingAmountSent - 1;

    if (len <= 0)
        return len;
    WebData webdata(data, len);
    if (m_socket->send(webdata)) {
        m_pendingAmountSent += len;
        LOG(Network, "sent");
        return len;
    }
    LOG(Network, "busy. buffering");
    return 0;
}

void SocketStreamHandleInternal::close()
{
    LOG(Network, "close");
    m_socket->close();
}
    
void SocketStreamHandleInternal::didOpenStream(WebSocketStreamHandle* socketHandle, int maxPendingSendAllowed)
{
    LOG(Network, "SocketStreamHandleInternal::didOpen %d",
        maxPendingSendAllowed);
    ASSERT(maxPendingSendAllowed > 0);
    if (m_handle && m_socket.get()) {
        ASSERT(socketHandle == m_socket.get());
        m_maxPendingSendAllowed = maxPendingSendAllowed;
        m_handle->m_state = SocketStreamHandleBase::Open;
        if (m_handle->m_client) {
            m_handle->m_client->didOpen(m_handle);
            return;
        }
    }
    LOG(Network, "no m_handle or m_socket?");
}

void SocketStreamHandleInternal::didSendData(WebSocketStreamHandle* socketHandle, int amountSent)
{
    LOG(Network, "SocketStreamHandleInternal::didSendData %d", amountSent);
    ASSERT(amountSent > 0);
    if (m_handle && m_socket.get()) {
        ASSERT(socketHandle == m_socket.get());
        m_pendingAmountSent -= amountSent;
        ASSERT(m_pendingAmountSent >= 0);
        m_handle->sendPendingData();
    }
}

void SocketStreamHandleInternal::didReceiveData(WebSocketStreamHandle* socketHandle, const WebData& data)
{
    LOG(Network, "didReceiveData");
    if (m_handle && m_socket.get()) {
        ASSERT(socketHandle == m_socket.get());
        if (m_handle->m_client)
            m_handle->m_client->didReceiveData(m_handle, data.data(), data.size());
    }
}

void SocketStreamHandleInternal::didClose(WebSocketStreamHandle* socketHandle)
{
    LOG(Network, "didClose");
    if (m_handle && m_socket.get()) {
        ASSERT(socketHandle == m_socket.get());
        m_socket.clear();
        SocketStreamHandle* h = m_handle;
        m_handle = 0;
        if (h->m_client)
            h->m_client->didClose(h);
    }
}

void SocketStreamHandleInternal::didFail(WebSocketStreamHandle* socketHandle, const WebSocketStreamError& err)
{
    LOG(Network, "didFail");
    if (m_handle && m_socket.get()) {
        ASSERT(socketHandle == m_socket.get());
        m_socket.clear();
        SocketStreamHandle* h = m_handle;
        m_handle = 0;
        if (h->m_client)
            h->m_client->didClose(h);  // didFail(h, err);
    }
}

// FIXME: auth

// SocketStreamHandle ----------------------------------------------------------

SocketStreamHandle::SocketStreamHandle(const KURL& url, SocketStreamHandleClient* client)
    : SocketStreamHandleBase(url, client)
{
    m_internal = SocketStreamHandleInternal::create(this);
    m_internal->connect(m_url);
}

SocketStreamHandle::~SocketStreamHandle()
{
    setClient(0);
    m_internal.clear();
}

int SocketStreamHandle::platformSend(const char* buf, int len)
{
    if (!m_internal.get())
        return 0;
    return m_internal->send(buf, len);
}

void SocketStreamHandle::platformClose()
{
    if (m_internal.get())
        m_internal->close();
}

void SocketStreamHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    if (m_client)
        m_client->didReceiveAuthenticationChallenge(this, challenge);
}

void SocketStreamHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    notImplemented();
}

void SocketStreamHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    notImplemented();
}

}  // namespace WebCore

#endif  // ENABLE(WEB_SOCKETS)
