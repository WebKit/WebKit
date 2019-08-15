/*
 * Copyright (C) 2009, 2011, 2012 Google Inc.  All rights reserved.
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

#include "CookieRequestHeaderFieldProxy.h"
#include "SocketStreamHandleClient.h"
#include <wtf/Function.h>

namespace WebCore {

SocketStreamHandle::SocketStreamHandle(const URL& url, SocketStreamHandleClient& client)
    : m_url(url)
    , m_client(client)
    , m_state(Connecting)
{
    ASSERT(isMainThread());
}

SocketStreamHandle::SocketStreamState SocketStreamHandle::state() const
{
    return m_state;
}

void SocketStreamHandle::sendData(const char* data, size_t length, Function<void(bool)> completionHandler)
{
    if (m_state == Connecting || m_state == Closing)
        return completionHandler(false);
    platformSend(reinterpret_cast<const uint8_t*>(data), length, WTFMove(completionHandler));
}

void SocketStreamHandle::sendHandshake(CString&& handshake, Optional<CookieRequestHeaderFieldProxy>&& headerFieldProxy, Function<void(bool, bool)> completionHandler)
{
    if (m_state == Connecting || m_state == Closing)
        return completionHandler(false, false);
    platformSendHandshake(reinterpret_cast<const uint8_t*>(handshake.data()), handshake.length(), WTFMove(headerFieldProxy), WTFMove(completionHandler));
}

void SocketStreamHandle::close()
{
    if (m_state == Closed)
        return;
    m_state = Closing;
    if (bufferedAmount())
        return;
    disconnect();
}

void SocketStreamHandle::disconnect()
{
    auto protect = makeRef(static_cast<SocketStreamHandle&>(*this)); // platformClose calls the client, which may make the handle get deallocated immediately.

    platformClose();
    m_state = Closed;
}

} // namespace WebCore
