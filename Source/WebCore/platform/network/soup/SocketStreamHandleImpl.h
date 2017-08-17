/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 * Copyright (C) 2012 Samsung Electronics Ltd. All Rights Reserved.
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

#include "SocketStreamHandle.h"

#if USE(SOUP)

#include <pal/SessionID.h>
#include <wtf/StreamBuffer.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class SocketStreamError;
class SocketStreamHandleClient;

class SocketStreamHandleImpl final : public SocketStreamHandle {
public:
    static Ref<SocketStreamHandleImpl> create(const URL&, SocketStreamHandleClient&, PAL::SessionID, const String&, SourceApplicationAuditToken&&);
    static Ref<SocketStreamHandle> create(GSocketConnection*, SocketStreamHandleClient&);

    virtual ~SocketStreamHandleImpl();

    void platformSend(const char* data, size_t length, Function<void(bool)>&&) final;
    void platformClose() final;
private:
    SocketStreamHandleImpl(const URL&, SocketStreamHandleClient&);

    size_t bufferedAmount() final;
    std::optional<size_t> platformSendInternal(const char*, size_t);
    bool sendPendingData();

    void beginWaitingForSocketWritability();
    void stopWaitingForSocketWritability();

    static void connectedCallback(GSocketClient*, GAsyncResult*, SocketStreamHandleImpl*);
    static void readReadyCallback(GInputStream*, GAsyncResult*, SocketStreamHandleImpl*);
    static gboolean writeReadyCallback(GPollableOutputStream*, SocketStreamHandleImpl*);

    void connected(GRefPtr<GSocketConnection>&&);
    void readBytes(gssize);
    void didFail(SocketStreamError&&);
    void writeReady();

    GRefPtr<GSocketConnection> m_socketConnection;
    GRefPtr<GInputStream> m_inputStream;
    GRefPtr<GPollableOutputStream> m_outputStream;
    GRefPtr<GSource> m_writeReadySource;
    GRefPtr<GCancellable> m_cancellable;
    std::unique_ptr<char[]> m_readBuffer;

    StreamBuffer<char, 1024 * 1024> m_buffer;
    static const unsigned maxBufferSize = 100 * 1024 * 1024;
};

} // namespace WebCore

#endif
