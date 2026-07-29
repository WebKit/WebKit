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

WebTransportReceiveStreamByteSource::WebTransportReceiveStreamByteSource(WebTransport& transport, WebTransportStreamIdentifier identifier)
    : m_transport(transport)
    , m_identifier(identifier)
{
}

// https://w3c.github.io/webtransport/#webtransportreceivestream-pull-bytes
void WebTransportReceiveStreamByteSource::pull(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller, Ref<DeferredPromise>&& promise)
{
    if (m_isCancelled || m_isClosed)
        return promise->resolve();

    if (m_queue.isEmpty()) {
        if (!m_finReceived) {
            ASSERT(!m_pendingPull);
            m_pendingPull = WTF::move(promise);
            return;
        }

        closeStream(globalObject, controller);
        promise->resolve();
        return;
    }

    deliverBytes(globalObject, controller, WTF::move(promise));
}

void WebTransportReceiveStreamByteSource::deliverBytes(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller, Ref<DeferredPromise>&& promise)
{
    ASSERT(!m_queue.isEmpty());
    Ref buffer = m_queue.takeFirst();
    auto byteLength = buffer->byteLength();
    ASSERT(m_currentOffset < byteLength);

    auto newOffset = controller.pullFromBytes(globalObject, buffer.get(), m_currentOffset);
    if (newOffset < byteLength) {
        m_queue.prepend(WTF::move(buffer));
        m_currentOffset = newOffset;
    } else
        m_currentOffset = 0;

    if (m_finReceived && m_queue.isEmpty())
        closeStream(globalObject, controller);

    promise->resolve();
}

void WebTransportReceiveStreamByteSource::closeStream(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller)
{
    ASSERT(m_queue.isEmpty());
    m_isClosed = true;
    controller.closeAndRespondToPendingPullIntos(globalObject);
    if (RefPtr transport = m_transport.get())
        transport->receiveStreamClosed(m_identifier);
}

void WebTransportReceiveStreamByteSource::errorStream(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller, const Exception& exception)
{
    m_isClosed = true;
    m_queue.clear();
    m_currentOffset = 0;

    controller.error(globalObject, exception);
    if (RefPtr transport = m_transport.get())
        transport->receiveStreamClosed(m_identifier);
    if (RefPtr pendingPull = std::exchange(m_pendingPull, nullptr))
        pendingPull->resolve();
}

void WebTransportReceiveStreamByteSource::receiveBytes(std::span<const uint8_t> bytes, bool withFin, std::optional<Exception>&& exception)
{
    if (withFin)
        m_finReceived = true;

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

    Locker<JSC::JSLock> locker(globalObject->vm().apiLock());

    if (exception)
        return errorStream(*globalObject, *controller, *exception);

    if (bytes.size()) {
        RefPtr arrayBuffer = ArrayBuffer::tryCreateUninitialized(bytes.size(), 1);
        if (!arrayBuffer)
            return errorStream(*globalObject, *controller, Exception { ExceptionCode::RangeError, "Unable to allocate memory for the received bytes"_s });
        memcpySpan(arrayBuffer->mutableSpan(), bytes);
        m_queue.append(arrayBuffer.releaseNonNull());
    }

    if (!m_queue.isEmpty()) {
        if (RefPtr pendingPull = std::exchange(m_pendingPull, nullptr))
            deliverBytes(*globalObject, *controller, pendingPull.releaseNonNull());
        return;
    }

    if (m_finReceived) {
        closeStream(*globalObject, *controller);
        if (RefPtr pendingPull = std::exchange(m_pendingPull, nullptr))
            pendingPull->resolve();
    }
}

void WebTransportReceiveStreamByteSource::receiveError(JSDOMGlobalObject& globalObject, JSC::JSValue error)
{
    if (m_isClosed || m_isCancelled)
        return;
    m_isCancelled = true;
    m_queue.clear();
    m_currentOffset = 0;

    Locker<JSC::JSLock> locker(globalObject.vm().apiLock());
    if (RefPtr stream = m_stream.get()) {
        if (RefPtr controller = stream->controller())
            controller->error(globalObject, error);
    }

    if (RefPtr pendingPull = std::exchange(m_pendingPull, nullptr))
        pendingPull->resolve();

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
    m_queue.clear();
    m_currentOffset = 0;
    if (RefPtr pendingPull = std::exchange(m_pendingPull, nullptr))
        pendingPull->resolve();

    RefPtr transport = m_transport.get();
    if (!transport)
        return;
    transport->receiveStreamClosed(m_identifier);

    std::optional<uint64_t> errorCode;
    if (auto* jsWebTransportError = dynamicDowncast<JSWebTransportError>(reason)) {
        auto& webTransportError = jsWebTransportError->wrapped();
        if (auto webTransportErrorCode = webTransportError.streamErrorCode())
            errorCode = static_cast<uint64_t>(*webTransportErrorCode);
    }
    transport->session()->cancelReceiveStream(m_identifier, errorCode);
}

}
