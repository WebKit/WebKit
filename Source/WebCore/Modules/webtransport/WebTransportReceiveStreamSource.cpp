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
#include "WebTransportReceiveStreamSource.h"

#include "JSDOMException.h"
#include "JSDOMGlobalObject.h"
#include "JSWebTransportReceiveStream.h"
#include "WebTransport.h"
#include "WebTransportError.h"
#include "WebTransportSession.h"

namespace WebCore {

WebTransportReceiveStreamSource::WebTransportReceiveStreamSource()
{
}

WebTransportReceiveStreamSource::WebTransportReceiveStreamSource(WebTransport& transport, WebTransportStreamIdentifier identifier)
    : m_transport(transport)
    , m_identifier(identifier)
{
}

bool WebTransportReceiveStreamSource::receiveIncomingStream(JSC::JSGlobalObject& globalObject, Ref<WebTransportReceiveStream>& stream)
{
    if (m_isCancelled || m_identifier)
        return false;
    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(&globalObject);
    Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());
    auto value = toJS(&globalObject, &jsDOMGlobalObject, stream.get());
    if (!controller().enqueue(value)) {
        doCancel();
        return false;
    }
    return true;
}

void WebTransportReceiveStreamSource::receiveBytes(std::span<const uint8_t> bytes, bool withFin, std::optional<WebCore::Exception>&& exception)
{
    if (m_isCancelled || m_isClosed || !m_identifier)
        return;
    if (exception) {
        controller().error(*exception);
        clean();
        return;
    }
    if (bytes.size()) {
        auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(bytes.size(), 1);
        if (arrayBuffer)
            memcpySpan(arrayBuffer->mutableSpan(), bytes);
        if (!controller().enqueue(WTFMove(arrayBuffer)))
            doCancel();
    }
    if (withFin) {
        m_isClosed = true;
        controller().close();
        clean();
    }
}

void WebTransportReceiveStreamSource::receiveError(JSC::JSGlobalObject& globalObject, uint64_t errorCode)
{
    if (m_isCancelled || m_isClosed || !m_identifier)
        return;

    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(&globalObject);
    Locker<JSC::JSLock> locker(jsDOMGlobalObject.vm().apiLock());

    auto error = WebTransportError::create(String(emptyString()), WebTransportErrorOptions {
        WebTransportErrorSource::Stream,
        static_cast<unsigned>(errorCode)
    });

    auto jsError = toJS(&globalObject, &jsDOMGlobalObject, error.get());
    controller().error(globalObject, jsError);
    clean();
    m_isCancelled = true;
}

void WebTransportReceiveStreamSource::doCancel()
{
    if (m_isCancelled)
        return;
    m_isCancelled = true;
    if (!m_identifier)
        return;
    RefPtr transport = m_transport.get();
    if (!transport)
        return;
    RefPtr session = transport->session();
    if (!session)
        return;
    // FIXME: Use error code from WebTransportError
    session->cancelReceiveStream(*m_identifier, std::nullopt);
}
}
