/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
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

#include "KURL.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "SocketStreamHandleClient.h"
#include "SocketStreamHandlePrivate.h"
#include <wtf/Vector.h>
#include <wtf/brew/ShellBrew.h>
#include <wtf/text/CString.h>

namespace WebCore {

static void socketStreamConnectCallback(void* user, int nError)
{
    SocketStreamHandlePrivate* p = reinterpret_cast<SocketStreamHandlePrivate*>(user);

    if (nError != AEE_NET_SUCCESS) {
        p->socketError(nError);
        return;
    }

    p->socketConnected();
}

static void getHostByNameCallback(void* user)
{
    SocketStreamHandlePrivate* p = reinterpret_cast<SocketStreamHandlePrivate*>(user);

    if (p->m_dns.nResult < 1 || p->m_dns.nResult > AEEDNSMAXADDRS) {
        p->socketError(p->m_dns.nResult);
        return;
    }

    p->connect();
}

static void socketReadableCallback(void* user)
{
    SocketStreamHandlePrivate* p = reinterpret_cast<SocketStreamHandlePrivate*>(user);
    p->socketReadyRead();
}

static INetMgr* networkManager()
{
    static INetMgr* s_netMgr;

    if (!s_netMgr) {
        IShell* shell = reinterpret_cast<AEEApplet*>(GETAPPINSTANCE())->m_pIShell;
        ISHELL_CreateInstance(shell, AEECLSID_NET, reinterpret_cast<void**>(&s_netMgr));
        ASSERT(s_netMgr);
    }

    return s_netMgr;
}

SocketStreamHandlePrivate::SocketStreamHandlePrivate(SocketStreamHandle* streamHandle, const KURL& url)
{
    m_streamHandle = streamHandle;
    m_isSecure = url.protocolIs("wss");

    m_socket.set(INETMGR_OpenSocket(networkManager(), AEE_SOCK_STREAM));
    if (!m_socket)
        return;

    if (m_isSecure)
        m_ssl = createInstance<ISSL>(AEECLSID_SSL);

    m_port = url.hasPort() ? url.port() : (m_isSecure ? 443 : 80);

    CALLBACK_Init(&m_dnsCallback, getHostByNameCallback, this);
    m_dnsCallback.pfnCancel = 0;

    INETMGR_GetHostByName(networkManager(), &m_dns, url.host().latin1().data(), &m_dnsCallback);
}

SocketStreamHandlePrivate::~SocketStreamHandlePrivate()
{
}

void SocketStreamHandlePrivate::socketConnected()
{
    if (m_streamHandle && m_streamHandle->client()) {
        m_streamHandle->m_state = SocketStreamHandleBase::Open;
        m_streamHandle->client()->didOpen(m_streamHandle);
    }

    ISOCKET_Readable(m_socket.get(), socketReadableCallback, this);
}

void SocketStreamHandlePrivate::socketReadyRead()
{
    if (m_streamHandle && m_streamHandle->client()) {
        Vector<char> buffer(1024);

        int readSize = ISOCKET_Read(m_socket.get(), buffer.data(), buffer.size());
        if (readSize == AEE_NET_ERROR) {
            socketError(ISOCKET_GetLastError(m_socket.get()));
            return;
        }

        m_streamHandle->client()->didReceiveData(m_streamHandle, buffer.data(), readSize);
    }

    ISOCKET_Readable(m_socket.get(), socketReadableCallback, this);
}

void SocketStreamHandlePrivate::connect()
{
    ISOCKET_Connect(m_socket.get(), m_dns.addrs[0], HTONS(m_port), socketStreamConnectCallback, this);
}

int SocketStreamHandlePrivate::send(const char* data, int len)
{
    if (!m_socket)
        return 0;

    int sentSize = ISOCKET_Write(m_socket.get(), reinterpret_cast<byte*>(const_cast<char*>(data)), len);
    if (sentSize == AEE_NET_ERROR) {
        socketError(ISOCKET_GetLastError(m_socket.get()));
        return 0;
    }

    return sentSize;
}

void SocketStreamHandlePrivate::close()
{
    m_socket.clear();
}

void SocketStreamHandlePrivate::socketClosed()
{
    if (m_streamHandle && m_streamHandle->client()) {
        SocketStreamHandle* streamHandle = m_streamHandle;
        m_streamHandle = 0;
        // This following call deletes _this_. Nothing should be after it.
        streamHandle->client()->didClose(streamHandle);
    }
}

void SocketStreamHandlePrivate::socketError(int error)
{
    // FIXME - in the future, we might not want to treat all errors as fatal.
    if (m_streamHandle && m_streamHandle->client()) {
        SocketStreamHandle* streamHandle = m_streamHandle;
        m_streamHandle = 0;
        // This following call deletes _this_. Nothing should be after it.
        streamHandle->client()->didClose(streamHandle);
    }
}

SocketStreamHandle::SocketStreamHandle(const KURL& url, SocketStreamHandleClient* client)
    : SocketStreamHandleBase(url, client)
{
    LOG(Network, "SocketStreamHandle %p new client %p", this, m_client);
    m_p = new SocketStreamHandlePrivate(this, url);
}

SocketStreamHandle::~SocketStreamHandle()
{
    LOG(Network, "SocketStreamHandle %p delete", this);
    setClient(0);
    delete m_p;
}

int SocketStreamHandle::platformSend(const char* data, int len)
{
    LOG(Network, "SocketStreamHandle %p platformSend", this);
    return m_p->send(data, len);
}

void SocketStreamHandle::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);
    m_p->close();
}

void SocketStreamHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void SocketStreamHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}

} // namespace WebCore
