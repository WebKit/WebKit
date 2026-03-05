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
#include "ReadableStreamDefaultReader.h"

#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSReadableStreamDefaultReader.h"
#include "JSReadableStreamReadResult.h"
#include "ReadableByteStreamController.h"
#include "ReadableStream.h"
#include "ReadableStreamReadRequest.h"
#include "WebCoreOpaqueRootInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ReadableStreamDefaultReader);

ExceptionOr<Ref<ReadableStreamDefaultReader>> ReadableStreamDefaultReader::create(JSDOMGlobalObject& globalObject, ReadableStream& stream)
{
    if (RefPtr internalReadableStream = stream.internalReadableStream()) {
        auto internalReaderOrException = InternalReadableStreamDefaultReader::create(globalObject, *internalReadableStream);
        if (internalReaderOrException.hasException())
            return internalReaderOrException.releaseException();

        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        Ref reader = adoptRef(*new ReadableStreamDefaultReader(stream, internalReaderOrException.releaseReturnValue(), WTF::move(promise), WTF::move(deferred)));
        stream.setDefaultReader(reader.ptr());

        return reader;
    }

    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    Ref reader = adoptRef(*new ReadableStreamDefaultReader(stream, { }, WTF::move(promise), WTF::move(deferred)));

    auto result = reader->setup(globalObject);
    if (result.hasException())
        return result.releaseException();

    return reader;
}

ReadableStreamDefaultReader::ReadableStreamDefaultReader(Ref<ReadableStream>&& stream, RefPtr<InternalReadableStreamDefaultReader>&& internalDefaultReader, Ref<DOMPromise>&& promise, Ref<DeferredPromise>&& deferred)
    : m_closedPromise(WTF::move(promise))
    , m_closedDeferred(WTF::move(deferred))
    , m_stream(WTF::move(stream))
    , m_internalDefaultReader(WTF::move(internalDefaultReader))
{
    ASSERT(m_stream->hasByteStreamController() == !m_internalDefaultReader);
}

ReadableStreamDefaultReader::~ReadableStreamDefaultReader()
{
    RefPtr stream = m_stream;
    if (stream && stream->defaultReader() == this)
        stream->setDefaultReader(nullptr);
}

// https://streams.spec.whatwg.org/#generic-reader-closed
DOMPromise& ReadableStreamDefaultReader::closedPromise() const
{
    return m_closedPromise;
}

// https://streams.spec.whatwg.org/#default-reader-read
void ReadableStreamDefaultReader::readForBindings(JSDOMGlobalObject& globalObject, Ref<DeferredPromise>&& promise)
{
    read(globalObject, ReadableStreamReadRequest::create(WTF::move(promise)));
}

void ReadableStreamDefaultReader::read(JSDOMGlobalObject& globalObject, Ref<ReadableStreamReadRequest>&& readRequest)
{
    if (RefPtr internalReader = this->internalDefaultReader()) {
        auto value = internalReader->readForBindings(globalObject);
        auto* promise = jsCast<JSC::JSPromise*>(value);
        if (!promise)
            return;

        Ref domPromise = DOMPromise::create(globalObject, *promise);
        domPromise->whenSettledWithResult([domPromise, readRequest = WTF::move(readRequest)](auto* globalObject, bool isFulfilled, auto promiseResult) {
            if (!isFulfilled) {
                readRequest->runErrorSteps(promiseResult);
                return;
            }

            if (!globalObject)
                return;

            Ref vm = globalObject->vm();
            auto scope = DECLARE_THROW_SCOPE(vm);
            auto resultOrException = convertDictionary<ReadableStreamReadResult>(*globalObject, promiseResult);
            ASSERT(!resultOrException.hasException(scope));
            if (resultOrException.hasException(scope)) {
                TRY_CLEAR_EXCEPTION(scope, void());
                return;
            }
            auto result = resultOrException.releaseReturnValue();
            if (result.done) {
                readRequest->runCloseSteps();
                return;
            }
            readRequest->runChunkSteps(result.value);
        });
        return;
    }

    RefPtr stream = m_stream;
    if (!stream) {
        readRequest->runErrorSteps(Exception { ExceptionCode::TypeError, "stream is undefined"_s });
        return;
    }

    // https://streams.spec.whatwg.org/#readable-stream-default-reader-read
    ASSERT(stream->defaultReader() == this);
    ASSERT(stream->hasByteStreamController());

    stream->markAsDisturbed();
    switch (stream->state()) {
    case ReadableStream::State::Closed:
        readRequest->runCloseSteps();
        break;
    case ReadableStream::State::Errored:
        readRequest->runErrorSteps(stream->storedError(globalObject));
        break;
    case ReadableStream::State::Readable:
        RefPtr { stream->controller() }->runPullSteps(globalObject, WTF::move(readRequest));
    }
}

// https://streams.spec.whatwg.org/#default-reader-release-lock
ExceptionOr<void> ReadableStreamDefaultReader::releaseLock(JSDOMGlobalObject& globalObject)
{
    if (!m_stream)
        return { };

    if (RefPtr internalReader = this->internalDefaultReader()) {
        auto result = internalReader->releaseLock();
        if (!result.hasException()) {
            RefPtr stream = std::exchange(m_stream, { });
            stream->setDefaultReader(nullptr);
            stream = nullptr;
        }
        return result;
    }

    genericRelease(globalObject);
    errorReadRequests(Exception { ExceptionCode::TypeError, "lock released"_s });
    return { };
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-reader
ExceptionOr<void> ReadableStreamDefaultReader::setup(JSDOMGlobalObject& globalObject)
{
    RefPtr stream = m_stream;

    if (!stream)
        return Exception { ExceptionCode::TypeError, "stream is undefined"_s };

    if (stream->isLocked())
        return Exception { ExceptionCode::TypeError, "ReadableStream is locked"_s };

    stream->setDefaultReader(this);

    switch (stream->state()) {
    case ReadableStream::State::Readable:
        break;
    case ReadableStream::State::Closed:
        resolveClosedPromise();
        break;
    case ReadableStream::State::Errored:
        rejectClosedPromise(stream->storedError(globalObject));
        break;
    }

    return { };
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
void ReadableStreamDefaultReader::genericRelease(JSDOMGlobalObject& globalObject)
{
    RefPtr stream = m_stream;

    ASSERT(stream);
    ASSERT(stream->defaultReader() == this);
    if (!stream) [[unlikely]]
        return;

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

    stream->setDefaultReader(nullptr);
    m_stream = nullptr;
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
void ReadableStreamDefaultReader::errorReadRequests(const Exception& exception)
{
    auto readRequests = std::exchange(m_readRequests, { });
    for (auto& readRequest : readRequests)
        readRequest->runErrorSteps(Exception { exception });
}

// https://streams.spec.whatwg.org/#generic-reader-cancel
Ref<DOMPromise> ReadableStreamDefaultReader::cancel(JSDOMGlobalObject& globalObject, JSC::JSValue value)
{
    if (!m_stream) {
        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        deferred->reject(Exception { ExceptionCode::TypeError, "no stream"_s });
        return promise;
    }

    return genericCancel(globalObject, value);
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
Ref<DOMPromise> ReadableStreamDefaultReader::genericCancel(JSDOMGlobalObject& globalObject, JSC::JSValue value)
{
    RefPtr stream = m_stream;

    ASSERT(stream);
    ASSERT(stream->defaultReader() == this);
    if (!stream) [[unlikely]] {
        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        deferred->reject(Exception { ExceptionCode::TypeError, "no stream"_s });
        return promise;
    }

    return stream->cancel(globalObject, value);
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
void ReadableStreamDefaultReader::errorReadRequests(JSC::JSValue reason)
{
    auto readRequests = std::exchange(m_readRequests, { });
    for (auto& request : readRequests)
        request->runErrorSteps(reason);
}

void ReadableStreamDefaultReader::addReadRequest(Ref<ReadableStreamReadRequest>&& promise)
{
    m_readRequests.append(WTF::move(promise));
}

Ref<ReadableStreamReadRequest> ReadableStreamDefaultReader::takeFirstReadRequest()
{
    return m_readRequests.takeFirst();
}

void ReadableStreamDefaultReader::resolveClosedPromise()
{
    Ref { m_closedDeferred }->resolve();
}

void ReadableStreamDefaultReader::rejectClosedPromise(JSC::JSValue reason)
{
    Ref { m_closedDeferred }->reject<IDLAny>(reason, RejectAsHandled::Yes);
}

void ReadableStreamDefaultReader::onClosedPromiseRejection(ClosedRejectionCallback&& callback)
{
    if (m_internalDefaultReader) {
        m_internalDefaultReader->onClosedPromiseRejection(WTF::move(callback));
        return;
    }

    if (m_closedRejectionCallback) {
        auto oldCallback = std::exchange(m_closedRejectionCallback, { });
        m_closedRejectionCallback = [oldCallback = WTF::move(oldCallback), callback = WTF::move(callback)](auto& globalObject, auto value) mutable {
            oldCallback(globalObject, value);
            callback(globalObject, value);
        };
        return;
    }

    m_closedRejectionCallback = WTF::move(callback);
    Ref { m_closedPromise }->whenSettled([weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        Ref closedPromise = protectedThis->m_closedPromise;
        if (!closedPromise->globalObject() || !protectedThis->m_closedRejectionCallback || closedPromise->status() != DOMPromise::Status::Rejected)
            return;

        protectedThis->m_closedRejectionCallback(*closedPromise->globalObject(), closedPromise->result());
    });
}

void ReadableStreamDefaultReader::onClosedPromiseResolution(Function<void()>&& callback)
{
    if (m_internalDefaultReader) {
        m_internalDefaultReader->onClosedPromiseResolution(WTF::move(callback));
        return;
    }

    if (m_closedResolutionCallback) {
        auto oldCallback = std::exchange(m_closedResolutionCallback, { });
        m_closedResolutionCallback = [oldCallback = WTF::move(oldCallback), callback = WTF::move(callback)]() mutable {
            oldCallback();
            callback();
        };
        return;
    }

    m_closedResolutionCallback = WTF::move(callback);
    Ref { m_closedPromise }->whenSettled([weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        Ref closedPromise = protectedThis->m_closedPromise;
        if (!closedPromise->globalObject() || !protectedThis->m_closedResolutionCallback || closedPromise->status() != DOMPromise::Status::Fulfilled)
            return;

        // We exhange m_closedResolutionCallback to reset it to an empty function, which will deallocate any captured variable of the callback.
        std::exchange(protectedThis->m_closedResolutionCallback, { })();
    });
}

bool ReadableStreamDefaultReader::isReachableFromOpaqueRoots() const
{
    return getNumReadRequests() && m_stream && m_stream->isReachableFromOpaqueRoots();
}

JSC::JSValue JSReadableStreamDefaultReader::read(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalDefaultReader = wrapped().internalDefaultReader();
    if (!internalDefaultReader) {
        return callPromiseFunction(globalObject, callFrame, [this](auto& globalObject, auto&, auto&& promise) {
            wrapped().readForBindings(globalObject, WTF::move(promise));
        });
    }

    return internalDefaultReader->readForBindings(globalObject);
}

JSC::JSValue JSReadableStreamDefaultReader::closed(JSC::JSGlobalObject& globalObject) const
{
    RefPtr internalDefaultReader = wrapped().internalDefaultReader();
    if (!internalDefaultReader)
        return wrapped().closedPromise().promise();

    return internalDefaultReader->closedForBindings(globalObject);
}

WebCoreOpaqueRoot root(ReadableStreamDefaultReader* reader)
{
    return WebCoreOpaqueRoot { reader };
}

bool JSReadableStreamDefaultReaderOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, ASCIILiteral* reason)
{
    auto* jsReader = jsCast<JSReadableStreamDefaultReader*>(handle.slot()->asCell());
    SUPPRESS_UNCOUNTED_LOCAL auto& reader = jsReader->wrapped();
    SUPPRESS_UNCOUNTED_LOCAL if (reader.isReachableFromOpaqueRoots()) {
        if (reason) [[unlikely]]
            *reason = "ReadableStreamDefaultReader is reachable from opaque root"_s;
        return true;
    }

    return containsWebCoreOpaqueRoot(visitor, reader);
}

template<typename Visitor>
void ReadableStreamDefaultReader::visitAdditionalChildren(Visitor& visitor)
{
    if (m_stream)
        SUPPRESS_UNCOUNTED_ARG m_stream->visitAdditionalChildren(visitor);
}

template<typename Visitor>
void JSReadableStreamDefaultReader::visitAdditionalChildren(Visitor& visitor)
{
    // Do not ref `wrapped()` here since this function may get called on a GC thread.
    wrapped().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSReadableStreamDefaultReader);

} // namespace WebCore
