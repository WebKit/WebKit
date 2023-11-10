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

#include "config.h"
#include "WebTransportDatagramDuplexStream.h"

#include "JSDOMPromise.h"
#include "ReadableStream.h"
#include "WritableStream.h"

namespace WebCore {

Ref<WebTransportDatagramDuplexStream> WebTransportDatagramDuplexStream::create(Ref<ReadableStream>&& readable, Ref<WritableStream>&& writable)
{
    return adoptRef(*new WebTransportDatagramDuplexStream(WTFMove(readable), WTFMove(writable)));
}

WebTransportDatagramDuplexStream::WebTransportDatagramDuplexStream(Ref<ReadableStream>&& readable, Ref<WritableStream>&& writable)
    : m_readable(WTFMove(readable))
    , m_writable(WTFMove(writable))
{
}

WebTransportDatagramDuplexStream::~WebTransportDatagramDuplexStream() = default;

ReadableStream& WebTransportDatagramDuplexStream::readable()
{
    return m_readable.get();
}

WritableStream& WebTransportDatagramDuplexStream::writable()
{
    return m_writable.get();
}

unsigned WebTransportDatagramDuplexStream::maxDatagramSize()
{
    return m_outgoingMaxDatagramSize;
}

double WebTransportDatagramDuplexStream::incomingMaxAge()
{
    return m_incomingDatagramsExpirationDuration;
}

double WebTransportDatagramDuplexStream::outgoingMaxAge()
{
    return m_outgoingDatagramsExpirationDuration;
}

double WebTransportDatagramDuplexStream::incomingHighWaterMark()
{
    return m_incomingDatagramsHighWaterMark;
}

double WebTransportDatagramDuplexStream::outgoingHighWaterMark()
{
    return m_outgoingDatagramsHighWaterMark;
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setIncomingMaxAge(double maxAge)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-incomingmaxage
    if (std::isnan(maxAge) || maxAge < 0)
        return Exception { ExceptionCode::RangeError };
    if (!maxAge)
        maxAge = std::numeric_limits<double>::infinity();
    m_incomingDatagramsExpirationDuration = maxAge;
    return { };
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setOutgoingMaxAge(double maxAge)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-outgoingmaxage
    if (std::isnan(maxAge) || maxAge < 0)
        return Exception { ExceptionCode::RangeError };
    if (!maxAge)
        maxAge = std::numeric_limits<double>::infinity();
    m_outgoingDatagramsExpirationDuration = maxAge;
    return { };
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setIncomingHighWaterMark(double mark)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-incominghighwatermark
    if (std::isnan(mark) || mark < 0)
        return Exception { ExceptionCode::RangeError };
    if (mark < 1)
        mark = 1;
    m_incomingDatagramsHighWaterMark = mark;
    return { };
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setOutgoingHighWaterMark(double mark)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-outgoinghighwatermark
    if (std::isnan(mark) || mark < 0)
        return Exception { ExceptionCode::RangeError };
    if (mark < 1)
        mark = 1;
    m_outgoingDatagramsHighWaterMark = mark;
    return { };
}

}
