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

#include "ExceptionOr.h"
#include "JSDOMPromise.h"
#include "ReadableStream.h"
#include "WebTransport.h"
#include "WebTransportDatagramsWritable.h"
#include "WebTransportSession.h"
#include "WritableStream.h"

namespace WebCore {

Ref<WebTransportDatagramDuplexStream> WebTransportDatagramDuplexStream::create(Ref<ReadableStream>&& readable)
{
    return adoptRef(*new WebTransportDatagramDuplexStream(WTF::move(readable)));
}

WebTransportDatagramDuplexStream::WebTransportDatagramDuplexStream(Ref<ReadableStream>&& readable)
    : m_readable(WTF::move(readable))
{
}

WebTransportDatagramDuplexStream::~WebTransportDatagramDuplexStream() = default;

void WebTransportDatagramDuplexStream::attachTo(WebTransport& transport)
{
    ASSERT(!m_transport.get());
    m_transport = transport;
}

ExceptionOr<Ref<WebTransportDatagramsWritable>> WebTransportDatagramDuplexStream::createWritable(ScriptExecutionContext& context, WebTransportSendOptions&& options)
{
    return WebTransportDatagramsWritable::create(context, m_transport.get(), WTF::move(options));
}

RefPtr<WebTransportSession> WebTransportDatagramDuplexStream::session()
{
    RefPtr transport = m_transport.get();
    if (!transport)
        return nullptr;
    return transport->session();
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setIncomingMaxAge(std::optional<double> maxAge)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-incomingmaxage
    if (maxAge) {
        if (std::isnan(*maxAge) || maxAge < 0)
            return Exception { ExceptionCode::RangeError };
        if (!*maxAge)
            maxAge = std::nullopt;
    }
    m_incomingMaxAge = maxAge;
    if (RefPtr session = this->session())
        session->datagramIncomingMaxAgeUpdated(m_incomingMaxAge);
    return { };
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setOutgoingMaxAge(std::optional<double> maxAge)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-outgoingmaxage
    if (maxAge) {
        if (std::isnan(*maxAge) || maxAge < 0)
            return Exception { ExceptionCode::RangeError };
        if (!*maxAge)
            maxAge = std::nullopt;
    }
    m_outgoingMaxAge = maxAge;
    if (RefPtr session = this->session())
        session->datagramOutgoingMaxAgeUpdated(m_outgoingMaxAge);
    return { };
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setIncomingHighWaterMark(double mark)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-incominghighwatermark
    if (std::isnan(mark) || mark < 0)
        return Exception { ExceptionCode::RangeError };
    if (mark < 1)
        mark = 1;
    m_incomingHighWaterMark = mark;
    if (RefPtr session = this->session())
        session->datagramIncomingHighWaterMarkUpdated(m_incomingHighWaterMark);
    return { };
}

ExceptionOr<void> WebTransportDatagramDuplexStream::setOutgoingHighWaterMark(double mark)
{
    // https://www.w3.org/TR/webtransport/#dom-webtransportdatagramduplexstream-outgoinghighwatermark
    if (std::isnan(mark) || mark < 0)
        return Exception { ExceptionCode::RangeError };
    if (mark < 1)
        mark = 1;
    m_outgoingHighWaterMark = mark;
    if (RefPtr session = this->session())
        session->datagramOutgoingHighWaterMarkUpdated(m_outgoingHighWaterMark);
    return { };
}

}
