/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSDOMPromiseDeferred.h"
#include "WritableStreamSink.h"
#include <wtf/Deque.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class WebTransport;
class WebTransportDatagramsWritable;

class DatagramSink : public WritableStreamSink {
public:
    static Ref<DatagramSink> create(WebTransport* transport) { return adoptRef(*new DatagramSink(transport)); }
    ~DatagramSink();

    void NODELETE attachTo(WebTransportDatagramsWritable&);

private:
    explicit DatagramSink(WebTransport*);

    void write(ScriptExecutionContext&, JSC::JSValue, DOMPromiseDeferred<void>&&) final;
    void close(JSDOMGlobalObject&, DOMPromiseDeferred<void>&&) final;

    void datagramSent();
    void resolveBufferedDatagramsWithRoom();

    struct OutgoingDatagram {
        DOMPromiseDeferred<void> promise;
        bool settled { false };
    };
    Deque<OutgoingDatagram> m_outgoingQueue;

    ThreadSafeWeakPtr<WebTransport> m_transport;
    WeakPtr<WebTransportDatagramsWritable> m_datagrams;
    bool m_isClosed { false };
};

}
