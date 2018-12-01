/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc.  All rights reserved.
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

#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/URL.h>

namespace WebCore {

struct CookieRequestHeaderFieldProxy;
class SocketStreamHandleClient;

typedef struct {
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> sourceApplicationAuditData;
#else
    void *empty { nullptr };
#endif
} SourceApplicationAuditToken;

class SocketStreamHandle : public ThreadSafeRefCounted<SocketStreamHandle> {
public:
    enum SocketStreamState { Connecting, Open, Closing, Closed };
    virtual ~SocketStreamHandle() = default;
    SocketStreamState state() const;

    void sendData(const char* data, size_t length, Function<void(bool)>);
    void sendHandshake(CString&& handshake, std::optional<CookieRequestHeaderFieldProxy>&&, Function<void(bool, bool)>);
    void close(); // Disconnect after all data in buffer are sent.
    void disconnect();
    virtual size_t bufferedAmount() = 0;

protected:
    WEBCORE_EXPORT SocketStreamHandle(const URL&, SocketStreamHandleClient&);

    virtual void platformSend(const uint8_t* data, size_t length, Function<void(bool)>&&) = 0;
    virtual void platformSendHandshake(const uint8_t* data, size_t length, const std::optional<CookieRequestHeaderFieldProxy>&, Function<void(bool, bool)>&&) = 0;
    virtual void platformClose() = 0;

    URL m_url;
    SocketStreamHandleClient& m_client;
    SocketStreamState m_state;
};

} // namespace WebCore
