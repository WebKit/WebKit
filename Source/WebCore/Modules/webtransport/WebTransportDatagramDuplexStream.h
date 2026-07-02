/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include <wtf/Deque.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class DOMPromise;
class ReadableStream;
class ScriptExecutionContext;
class WebTransport;
class WebTransportDatagramsWritable;
class WebTransportSession;
struct WebTransportSendOptions;
template<typename> class ExceptionOr;

class WebTransportDatagramDuplexStream : public RefCounted<WebTransportDatagramDuplexStream> {
public:
    static Ref<WebTransportDatagramDuplexStream> create(Ref<ReadableStream>&&);
    ~WebTransportDatagramDuplexStream();

    ReadableStream& readable() { return m_readable; }
    ExceptionOr<Ref<WebTransportDatagramsWritable>> createWritable(ScriptExecutionContext&, WebTransportSendOptions&&);
    unsigned maxDatagramSize() const { return std::numeric_limits<uint16_t>::max(); }
    std::optional<double> incomingMaxAge() const { return m_incomingMaxAge; }
    std::optional<double> outgoingMaxAge() const { return m_outgoingMaxAge; }
    double incomingHighWaterMark() const { return m_incomingHighWaterMark; }
    double outgoingHighWaterMark() const { return m_outgoingHighWaterMark; }
    ExceptionOr<void> setIncomingMaxAge(std::optional<double>);
    ExceptionOr<void> setOutgoingMaxAge(std::optional<double>);
    ExceptionOr<void> setIncomingHighWaterMark(double);
    ExceptionOr<void> setOutgoingHighWaterMark(double);

    void attachTo(WebTransport&);

private:
    WebTransportDatagramDuplexStream(Ref<ReadableStream>&&);

    RefPtr<WebTransportSession> session();

    const Ref<ReadableStream> m_readable;
    double m_incomingHighWaterMark { 1 };
    double m_outgoingHighWaterMark { 1 };
    std::optional<double> m_incomingMaxAge;
    std::optional<double> m_outgoingMaxAge;
    ThreadSafeWeakPtr<WebTransport> m_transport;
};

}
