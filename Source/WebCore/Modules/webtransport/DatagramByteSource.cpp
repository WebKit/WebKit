/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DatagramByteSource.h"

#include "JSDOMGlobalObject.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableByteStreamController.h"
#include "ReadableStream.h"
#include "ReadableStreamBYOBRequest.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

DatagramByteSource::DatagramByteSource()
{
}

DatagramByteSource::~DatagramByteSource() = default;

void DatagramByteSource::receiveDatagram(std::span<const uint8_t> datagram, bool withFin, std::optional<Exception>&& exception)
{
    if (m_isCancelled || m_isClosed)
        return;

    if (exception) {
        m_exception = WTFMove(exception);
        closeStreamIfPossible();
        return;
    }

    auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(datagram.size(), 1);
    if (!arrayBuffer) {
        // FIXME: error the stream.
        return;
    }

    memcpySpan(arrayBuffer->mutableSpan(), datagram);

    if (!m_promise || !m_queue.isEmpty()) {
        // FIXME: https://www.w3.org/TR/webtransport/#receivedatagrams tells us to remove datagrams:
        // - If incomingHighWaterMark is reached
        // - If datagrams are too old.
        m_queue.append(arrayBuffer.releaseNonNull());
        m_isClosed = withFin;
        return;
    }

    RefPtr controller = m_controller;
    auto* globalObject = controller->protectedStream()->globalObject();
    if (!globalObject)
        return;

    ASSERT(!m_currentOffset);
    tryEnqueuing(*arrayBuffer, *controller, m_promise.releaseNonNull().get(), globalObject);
    if (!withFin)
        return;

    m_isClosed = true;
    closeStreamIfPossible();
}

void DatagramByteSource::pull(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller, Ref<DeferredPromise>&& promise)
{
    if (closeStreamIfNeeded(globalObject, controller, promise.get()))
        return;

    if (m_queue.isEmpty()) {
        m_promise = WTFMove(promise);
        m_controller = &controller;
        return;
    }

    tryEnqueuing(m_queue.takeFirst().get(), controller, WTFMove(promise), &globalObject);
}

void DatagramByteSource::cancel(Ref<DeferredPromise>&& promise)
{
    m_isCancelled = true;
    m_queue.clear();
    m_promise = nullptr;
    m_controller = nullptr;
    promise->resolve();
}

void DatagramByteSource::closeStreamIfPossible()
{
    RefPtr promise = std::exchange(m_promise, { });
    if (!promise)
        return;

    RefPtr controller = m_controller;
    auto* globalObject = controller->protectedStream()->globalObject();
    if (!globalObject)
        return;

    closeStream(*globalObject, *controller, *promise);
}

bool DatagramByteSource::closeStreamIfNeeded(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller, DeferredPromise& promise)
{
    if (!m_isClosed || !m_queue.isEmpty())
        return false;

    closeStream(globalObject, controller, promise);
    return true;
}

void DatagramByteSource::closeStream(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller, DeferredPromise& promise)
{
    if (m_exception)
        controller.error(globalObject, *m_exception);
    else
        controller.closeAndRespondToPendingPullIntos(globalObject);

    promise.resolve();
}

void DatagramByteSource::tryEnqueuing(JSC::ArrayBuffer& buffer, ReadableByteStreamController& controller, Ref<DeferredPromise>&& promise, JSDOMGlobalObject* globalObject)
{
    if (!globalObject) {
        globalObject = controller.protectedStream()->globalObject();
        if (!globalObject) {
            // FIXME: We should probably error.
            promise->resolve();
            return;
        }
    }

    size_t byteLength = buffer.byteLength();
    ASSERT(byteLength > m_currentOffset);
    if (RefPtr request = controller.getByobRequest()) {
        RefPtr view = request->view();
        // If view’s byte length is less than the size of datagram, return a promise rejected with a RangeError.
        if (view->byteLength() < byteLength - m_currentOffset) {
            promise->reject(Exception { ExceptionCode::RangeError, "BYOB request buffer is too small"_s });
            return;
        }

        size_t elementSize = 0;
        auto viewType = view->getType();
        if (viewType != JSC::TypedArrayType::TypeDataView)
            elementSize = JSC::elementSize(viewType);

        if (elementSize != 1) {
            promise->reject(Exception { ExceptionCode::TypeError, "BYOB request view element size is not 1"_s });
            return;
        }
    }

    auto newOffset = controller.pullFromBytes(*globalObject, buffer, m_currentOffset);
    if (newOffset != byteLength) {
        m_queue.prepend(buffer);
        m_currentOffset = newOffset;
    } else
        m_currentOffset = 0;

    if (m_exception || (m_isClosed && m_queue.isEmpty()))
        closeStream(*globalObject, controller, WTFMove(promise));
    else
        promise->resolve();
}

void DatagramByteSource::error(JSC::JSGlobalObject& globalObject, JSC::JSValue value)
{
    if (RefPtr controller = m_controller) {
        auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(&globalObject);
        controller->error(jsDOMGlobalObject, value);
    }
}
}
