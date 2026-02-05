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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ReadableStreamBYOBReader.h"

#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSReadableStreamBYOBReader.h"
#include "ReadableByteStreamController.h"
#include "ReadableStream.h"
#include "ReadableStreamReadRequest.h"
#include "WebCoreOpaqueRootInlines.h"
#include <wtf/TZoneMallocInlines.h>
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ReadableStreamBYOBReader);

ExceptionOr<Ref<ReadableStreamBYOBReader>> ReadableStreamBYOBReader::create(JSDOMGlobalObject& globalObject, ReadableStream& stream)
{
    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    Ref reader = adoptRef(*new ReadableStreamBYOBReader(WTF::move(promise), WTF::move(deferred)));
    auto result = reader->setupBYOBReader(globalObject, stream);
    if (result.hasException())
        return result.releaseException();
    return reader;
}

ReadableStreamBYOBReader::ReadableStreamBYOBReader(Ref<DOMPromise>&& promise, Ref<DeferredPromise>&& deferred)
    : m_closedPromise(WTF::move(promise))
    , m_closedDeferred(WTF::move(deferred))
{
}

ReadableStreamBYOBReader::~ReadableStreamBYOBReader()
{
    RefPtr stream = m_stream;
    if (stream && stream->byobReader() == this)
        stream->setByobReader(nullptr);
}

DOMPromise& ReadableStreamBYOBReader::closedPromise()
{
    return m_closedPromise;
}

// https://streams.spec.whatwg.org/#byob-reader-read
void ReadableStreamBYOBReader::readForBindings(JSDOMGlobalObject& globalObject, JSC::ArrayBufferView& view, ReadOptions options, Ref<DeferredPromise>&& promise)
{
    if (!view.byteLength())
        return promise->reject(Exception { ExceptionCode::TypeError, "view byteLength is 0"_s });

    RefPtr buffer = view.possiblySharedBuffer();
    if (!buffer)
        return promise->reject(Exception { ExceptionCode::TypeError, "view's buffer is detached"_s });

    if (!buffer->byteLength())
        return promise->reject(Exception { ExceptionCode::TypeError, "view's buffer byteLength is 0"_s });

    if (!options.min)
        return promise->reject(Exception { ExceptionCode::TypeError, "options min is 0"_s });

    auto viewType = view.getType();
    if (viewType != JSC::TypedArrayType::TypeDataView) {
        if (options.min > view.byteLength() / JSC::elementSize(viewType))
            return promise->reject(Exception { ExceptionCode::RangeError, "view's buffer is not large enough"_s });
    } else {
        if (options.min > view.byteLength())
            return promise->reject(Exception { ExceptionCode::RangeError, "view's buffer is not large enough"_s });
    }

    if (!m_stream)
        return promise->reject(Exception { ExceptionCode::TypeError, "reader has no stream"_s });

    read(globalObject, view, options.min, ReadableStreamReadIntoRequest::create(WTF::move(promise)));
}

// https://streams.spec.whatwg.org/#byob-reader-release-lock
void ReadableStreamBYOBReader::releaseLock(JSDOMGlobalObject& globalObject)
{
    if (!m_stream)
        return;

    genericRelease(globalObject);

    errorReadIntoRequests(Exception { ExceptionCode::TypeError, "releasing stream"_s });
}

// https://streams.spec.whatwg.org/#generic-reader-cancel
Ref<DOMPromise> ReadableStreamBYOBReader::cancel(JSDOMGlobalObject& globalObject, JSC::JSValue value)
{
    if (!m_stream) {
        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        deferred->reject(Exception { ExceptionCode::TypeError, "no stream"_s });
        return promise;
    }

    return genericCancel(globalObject, value);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-byob-reader
ExceptionOr<void> ReadableStreamBYOBReader::setupBYOBReader(JSDOMGlobalObject& globalObject, ReadableStream& stream)
{
    if (stream.isLocked())
        return Exception { ExceptionCode::TypeError, "stream is locked"_s };

    if (!stream.hasByteStreamController())
        return Exception { ExceptionCode::TypeError, "stream is not a byte stream"_s };

    initialize(globalObject, stream);
    return { };
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-byob-reader
void ReadableStreamBYOBReader::initialize(JSDOMGlobalObject& globalObject, ReadableStream& stream)
{
    m_stream = &stream;

    stream.setByobReader(this);

    switch (stream.state()) {
    case ReadableStream::State::Readable:
        break;
    case ReadableStream::State::Closed:
        resolveClosedPromise();
        break;
    case ReadableStream::State::Errored:
        rejectClosedPromise(stream.storedError(globalObject));
        break;
    }
}

// https://streams.spec.whatwg.org/#readable-stream-byob-reader-read
void ReadableStreamBYOBReader::read(JSDOMGlobalObject& globalObject, JSC::ArrayBufferView& view, uint64_t optionMin, Ref<ReadableStreamReadIntoRequest>&& readRequest)
{
    ASSERT(m_stream);
    Ref stream = *m_stream;

    stream->markAsDisturbed();
    if (stream->state() == ReadableStream::State::Errored) {
        readRequest->runErrorSteps(stream->storedError(globalObject));
        return;
    }

    RefPtr controller = stream->controller();
    controller->pullInto(globalObject, view, optionMin, WTF::move(readRequest));
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
void ReadableStreamBYOBReader::genericRelease(JSDOMGlobalObject& globalObject)
{
    ASSERT(m_stream);
    Ref stream = *m_stream;

    ASSERT(stream->byobReader() == this);

    if (stream->state() == ReadableStream::State::Readable)
        Ref { m_closedDeferred }->reject(Exception { ExceptionCode::TypeError, "releasing stream"_s }, RejectAsHandled::Yes);
    else {
        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        deferred->reject(Exception { ExceptionCode::TypeError, "releasing stream"_s }, RejectAsHandled::Yes);
        m_closedDeferred = WTF::move(deferred);
        m_closedPromise = WTF::move(promise);
    }

    if (RefPtr controller = stream->controller())
        controller->runReleaseSteps();

    stream->setByobReader(nullptr);
    m_stream = nullptr;
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreambyobreadererrorreadintorequests
void ReadableStreamBYOBReader::errorReadIntoRequests(Exception&& exception)
{
    auto requests = std::exchange(m_readIntoRequests, { });
    for (auto& request : requests)
        request->runErrorSteps(Exception { exception });
}

void ReadableStreamBYOBReader::errorReadIntoRequests(JSC::JSValue reason)
{
    auto requests = std::exchange(m_readIntoRequests, { });
    for (auto& request : requests)
        request->runErrorSteps(reason);
}

void ReadableStreamBYOBReader::resolveClosedPromise()
{
    Ref { m_closedDeferred }->resolve();
}

void ReadableStreamBYOBReader::rejectClosedPromise(JSC::JSValue reason)
{
    Ref { m_closedDeferred }->reject<IDLAny>(reason, RejectAsHandled::Yes);
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
Ref<DOMPromise> ReadableStreamBYOBReader::genericCancel(JSDOMGlobalObject& globalObject, JSC::JSValue value)
{
    RefPtr stream = m_stream;

    ASSERT(stream);
    ASSERT(stream->byobReader() == this);

    return stream->cancel(globalObject, value);
}

Ref<ReadableStreamReadIntoRequest> ReadableStreamBYOBReader::takeFirstReadIntoRequest()
{
    return m_readIntoRequests.takeFirst();
}

void ReadableStreamBYOBReader::addReadIntoRequest(Ref<ReadableStreamReadIntoRequest>&& readIntoRequest)
{
    m_readIntoRequests.append(WTF::move(readIntoRequest));
}

void ReadableStreamBYOBReader::onClosedPromiseRejection(ClosedCallback&& callback)
{
    if (m_closedCallback) {
        auto oldCallback = std::exchange(m_closedCallback, { });
        m_closedCallback = [oldCallback = WTF::move(oldCallback), callback = WTF::move(callback)](auto& globalObject, auto value) mutable {
            oldCallback(globalObject, value);
            callback(globalObject, value);
        };
        return;
    }

    m_closedCallback = WTF::move(callback);
    Ref { m_closedPromise }->whenSettled([weakThis = WeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        Ref closedPromise = protectedThis->m_closedPromise;
        if (!closedPromise->globalObject() || !protectedThis->m_closedCallback || closedPromise->status() != DOMPromise::Status::Rejected)
            return;

        protectedThis->m_closedCallback(*closedPromise->globalObject(), closedPromise->result());
    });
}

bool ReadableStreamBYOBReader::isReachableFromOpaqueRoots() const
{
    return readIntoRequestsSize() && m_stream && m_stream->isReachableFromOpaqueRoots();
}

WebCoreOpaqueRoot root(ReadableStreamBYOBReader* reader)
{
    return WebCoreOpaqueRoot { reader };
}

bool JSReadableStreamBYOBReaderOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, ASCIILiteral* reason)
{
    auto* jsReader = jsCast<JSReadableStreamBYOBReader*>(handle.slot()->asCell());
    SUPPRESS_UNCOUNTED_LOCAL auto& reader = jsReader->wrapped();
    SUPPRESS_UNCOUNTED_LOCAL if (reader.isReachableFromOpaqueRoots()) {
        if (reason) [[unlikely]]
            *reason = "ReadableStreamBYOBReader is reachable from opaque root"_s;
        return true;
    }

    return containsWebCoreOpaqueRoot(visitor, reader);
}

template<typename Visitor>
void ReadableStreamBYOBReader::visitAdditionalChildren(Visitor& visitor)
{
    if (m_stream)
        SUPPRESS_UNCOUNTED_ARG m_stream->visitAdditionalChildren(visitor);
}

template<typename Visitor>
void JSReadableStreamBYOBReader::visitAdditionalChildren(Visitor& visitor)
{
    // Do not ref `wrapped()` here since this function may get called on the GC thread.
    SUPPRESS_UNCOUNTED_ARG wrapped().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSReadableStreamBYOBReader);

} // namespace WebCore
