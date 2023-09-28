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

#include "ExceptionOr.h"
#include <wtf/Deque.h>
#include <wtf/RefCounted.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class DOMPromise;
class ReadableStream;
class WritableStream;

class WebTransportDatagramDuplexStream : public RefCounted<WebTransportDatagramDuplexStream> {
public:
    static Ref<WebTransportDatagramDuplexStream> create(Ref<ReadableStream>&&, Ref<WritableStream>&&);
    ~WebTransportDatagramDuplexStream();

    ReadableStream& readable();
    WritableStream& writable();
    unsigned maxDatagramSize();
    double incomingMaxAge();
    double outgoingMaxAge();
    double incomingHighWaterMark();
    double outgoingHighWaterMark();
    ExceptionOr<void> setIncomingMaxAge(double);
    ExceptionOr<void> setOutgoingMaxAge(double);
    ExceptionOr<void> setIncomingHighWaterMark(double);
    ExceptionOr<void> setOutgoingHighWaterMark(double);

private:
    WebTransportDatagramDuplexStream(Ref<ReadableStream>&&, Ref<WritableStream>&&);

    Ref<ReadableStream> m_readable;
    Ref<WritableStream> m_writable;
    Deque<std::pair<Vector<uint8_t>, MonotonicTime>> m_incomingDatagramsQueue;
    RefPtr<DOMPromise> m_incomingDatagramsPullPromise;
    double m_incomingDatagramsHighWaterMark { 1 };
    double m_incomingDatagramsExpirationDuration { std::numeric_limits<double>::infinity() };
    Deque<std::tuple<Vector<uint8_t>, MonotonicTime, Ref<DOMPromise>>> m_outgoingDatagramsQueue;
    double m_outgoingDatagramsHighWaterMark { 1 };
    double m_outgoingDatagramsExpirationDuration { std::numeric_limits<double>::infinity() };
    size_t m_outgoingMaxDatagramSize { 1024 };
};

}
