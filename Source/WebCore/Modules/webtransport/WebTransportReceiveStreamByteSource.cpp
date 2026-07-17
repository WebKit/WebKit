/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WebTransportReceiveStreamByteSource.h"

#include "JSDOMException.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebTransportError.h"
#include "JSWebTransportReceiveStream.h"
#include "ReadableByteStreamController.h"
#include "ReadableStream.h"
#include "WebTransport.h"
#include "WebTransportError.h"
#include "WebTransportSession.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

Ref<DOMPromise> WebTransportReceiveStreamByteSource::pull(JSDOMGlobalObject& globalObject)
{
    // FIXME: This should implement https://w3c.github.io/webtransport/#webtransportreceivestream-pull-bytes
    // See DatagramByteSource::pull.
    auto* promise = JSC::JSPromise::resolvedPromise(&globalObject, JSC::jsUndefined());
    return DOMPromise::create(globalObject, *promise);
}

WebTransportReceiveStreamByteSource::WebTransportReceiveStreamByteSource(WebTransport& transport, WebTransportStreamIdentifier identifier)
    : m_transport(transport)
    , m_identifier(identifier)
{
}

void WebTransportReceiveStreamByteSource::receiveBytes(std::span<const uint8_t> bytes, bool withFin, std::optional<Exception>&& exception)
{
    if (m_isCancelled || m_isClosed)
        return;

    RefPtr stream = m_stream.get();
    if (!stream)
        return;
    auto* globalObject = stream->globalObject();
    if (!globalObject)
        return;
    RefPtr controller = stream->controller();
    if (!controller)
        return;

    if (exception) {
        controller->error(*globalObject, *exception);
        if (RefPtr transport = m_transport.get())
            transport->receiveStreamClosed(m_identifier);
        return;
    }

    if (bytes.size()) {
        if (RefPtr arrayBuffer = ArrayBuffer::tryCreateUninitialized(bytes.size(), 1)) {
            memcpySpan(arrayBuffer->mutableSpan(), bytes);
            controller->enqueue(*globalObject, *arrayBuffer);
        }
        // FIXME: Error the stream if allocation fails.
    }

    if (withFin) {
        m_isClosed = true;
        controller->closeAndRespondToPendingPullIntos(*globalObject);
        if (RefPtr transport = m_transport.get())
            transport->receiveStreamClosed(m_identifier);
    }
}

void WebTransportReceiveStreamByteSource::receiveError(JSDOMGlobalObject& globalObject, JSC::JSValue error)
{
    if (m_isClosed || m_isCancelled)
        return;
    m_isCancelled = true;

    Locker<JSC::JSLock> locker(globalObject.vm().apiLock());
    if (RefPtr stream = m_stream.get()) {
        if (RefPtr controller = stream->controller())
            controller->error(globalObject, error);
    }

    if (RefPtr transport = m_transport.get())
        transport->receiveStreamClosed(m_identifier);
}

void WebTransportReceiveStreamByteSource::cancel(JSC::JSValue reason, Ref<DeferredPromise>&& promise)
{
    auto scope = makeScopeExit([&] {
        promise->resolve();
    });

    if (m_isCancelled)
        return;
    m_isCancelled = true;

    RefPtr transport = m_transport.get();
    if (!transport)
        return;
    transport->receiveStreamClosed(m_identifier);

    RefPtr session = transport->session();
    if (!session)
        return;

    std::optional<uint64_t> errorCode;
    if (auto* jsWebTransportError = dynamicDowncast<JSWebTransportError>(reason)) {
        auto& webTransportError = jsWebTransportError->wrapped();
        if (auto webTransportErrorCode = webTransportError.streamErrorCode())
            errorCode = static_cast<uint64_t>(*webTransportErrorCode);
    }
    session->cancelReceiveStream(m_identifier, errorCode);
}

}
