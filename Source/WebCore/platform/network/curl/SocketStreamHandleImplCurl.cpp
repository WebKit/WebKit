/*
 * Copyright (C) 2009 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "SocketStreamHandleImpl.h"

#if USE(CURL)

#include "CurlStreamScheduler.h"
#include "DeprecatedGlobalSettings.h"
#include "SocketStreamError.h"
#include "SocketStreamHandleClient.h"
#include "StorageSessionProvider.h"

namespace WebCore {

SocketStreamHandleImpl::SocketStreamHandleImpl(const URL& url, SocketStreamHandleClient& client, const StorageSessionProvider* provider)
    : SocketStreamHandle(url, client)
    , m_storageSessionProvider(provider)
    , m_scheduler(CurlContext::singleton().streamScheduler())
{
    // FIXME: Using DeprecatedGlobalSettings from here is a layering violation.
    if (m_url.protocolIs("wss") && DeprecatedGlobalSettings::allowsAnySSLCertificate())
        CurlContext::singleton().sslHandle().setIgnoreSSLErrors(true);

    m_streamID = m_scheduler.createStream(m_url, *this);
}

SocketStreamHandleImpl::~SocketStreamHandleImpl()
{
    destructStream();
}

Optional<size_t> SocketStreamHandleImpl::platformSendInternal(const uint8_t* data, size_t length)
{
    if (isStreamInvalidated())
        return WTF::nullopt;

    if (m_totalSendDataSize + length > maxBufferSize)
        return 0;
    m_totalSendDataSize += length;

    auto buffer = makeUniqueArray<uint8_t>(length);
    memcpy(buffer.get(), data, length);

    m_scheduler.send(m_streamID, WTFMove(buffer), length);
    return length;
}

void SocketStreamHandleImpl::platformClose()
{
    destructStream();

    if (m_state == Closed)
        return;
    m_state = Closed;

    m_client.didCloseSocketStream(*this);
}

void SocketStreamHandleImpl::didOpen(CurlStreamID)
{
    if (m_state != Connecting)
        return;
    m_state = Open;

    m_client.didOpenSocketStream(*this);
}

void SocketStreamHandleImpl::didSendData(CurlStreamID, size_t length)
{
    ASSERT(m_totalSendDataSize - length >= 0);

    m_totalSendDataSize -= length;
    sendPendingData();
}

void SocketStreamHandleImpl::didReceiveData(CurlStreamID, const char* data, size_t length)
{
    if (m_state != Open)
        return;

    m_client.didReceiveSocketStreamData(*this, data, length);
}

void SocketStreamHandleImpl::didFail(CurlStreamID, CURLcode errorCode)
{
    destructStream();

    if (m_state == Closed)
        return;

    if (errorCode == CURLE_RECV_ERROR)
        m_client.didFailToReceiveSocketStreamData(*this);
    else
        m_client.didFailSocketStream(*this, SocketStreamError(errorCode, m_url.string(), CurlHandle::errorDescription(errorCode)));
}

void SocketStreamHandleImpl::destructStream()
{
    if (isStreamInvalidated())
        return;

    m_scheduler.destroyStream(m_streamID);
    m_streamID = invalidCurlStreamID;
}

} // namespace WebCore

#endif
