/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CurlStream.h"
#include "SocketStreamHandle.h"
#include <pal/SessionID.h>
#include <wtf/StreamBuffer.h>

namespace WebCore {

class SocketStreamHandleClient;
class StorageSessionProvider;

class SocketStreamHandleImpl : public SocketStreamHandle, public CurlStream::Client {
public:
    static Ref<SocketStreamHandleImpl> create(const URL& url, SocketStreamHandleClient& client, PAL::SessionID, const String&, SourceApplicationAuditToken&&, const StorageSessionProvider* provider, bool) { return adoptRef(*new SocketStreamHandleImpl(url, client, provider)); }

    virtual ~SocketStreamHandleImpl();

    WEBCORE_EXPORT void platformSend(const uint8_t* data, size_t length, Function<void(bool)>&&) final;
    WEBCORE_EXPORT void platformSendHandshake(const uint8_t* data, size_t length, const std::optional<CookieRequestHeaderFieldProxy>&, Function<void(bool, bool)>&&) final;
    WEBCORE_EXPORT void platformClose() final;

private:
    WEBCORE_EXPORT SocketStreamHandleImpl(const URL&, SocketStreamHandleClient&, const StorageSessionProvider*);

    size_t bufferedAmount() final;
    std::optional<size_t> platformSendInternal(const uint8_t*, size_t);
    bool sendPendingData();

    // CurlStream::Client
    void didOpen(CurlStreamID) final;
    void didSendData(CurlStreamID, size_t) final;
    void didReceiveData(CurlStreamID, const SharedBuffer&) final;
    void didFail(CurlStreamID, CURLcode) final;

    bool isStreamInvalidated() { return m_streamID == invalidCurlStreamID; }
    void destructStream();

    RefPtr<const StorageSessionProvider> m_storageSessionProvider;

    StreamBuffer<uint8_t, 1024 * 1024> m_buffer;
    static const unsigned maxBufferSize = 100 * 1024 * 1024;

    CurlStreamScheduler& m_scheduler;
    CurlStreamID m_streamID { invalidCurlStreamID };
    unsigned m_totalSendDataSize { 0 };
};

} // namespace WebCore
