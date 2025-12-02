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
#include "ReadableByteStreamController.h"

#include "JSDOMException.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSReadableByteStreamController.h"
#include "JSReadableStreamReadResult.h"
#include "ReadableStream.h"
#include "ReadableStreamBYOBReader.h"
#include "ReadableStreamBYOBRequest.h"
#include "ReadableStreamDefaultReader.h"
#include "UnderlyingSourceCancelCallback.h"
#include "UnderlyingSourcePullCallback.h"
#include "UnderlyingSourceStartCallback.h"
#include "WebCoreOpaqueRootInlines.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ReadableByteStreamController);

template<typename Algorithm, typename AlgorithmParameter>
Ref<DOMPromise> getAlgorithmPromise(JSDOMGlobalObject& globalObject, RefPtr<Algorithm> algorithm, JSC::JSValue underlyingSource, AlgorithmParameter&& parameter)
{
    RefPtr<DOMPromise> algorithmPromise;
    if (!algorithm) {
        auto* promise = JSC::JSPromise::resolvedPromise(&globalObject, JSC::jsUndefined());
        algorithmPromise = DOMPromise::create(globalObject, *promise);
    } else {
        auto algorithmResult = algorithm->invoke(underlyingSource, parameter);
        if (algorithmResult.type() != CallbackResultType::Success) {
            auto* promise = JSC::JSPromise::rejectedPromise(&globalObject, JSC::jsUndefined());
            algorithmPromise = DOMPromise::create(globalObject, *promise);
        } else
            algorithmPromise = algorithmResult.releaseReturnValue();
    }
    return algorithmPromise.releaseNonNull();
}

ReadableByteStreamController::ReadableByteStreamController(ReadableStream& stream, JSC::JSValue underlyingSource, RefPtr<UnderlyingSourcePullCallback>&& pullAlgorithm, RefPtr<UnderlyingSourceCancelCallback>&& cancelAlgorithm, double highWaterMark, size_t autoAllocateChunkSize)
    : m_stream(stream)
    , m_strategyHWM(highWaterMark)
    , m_pullAlgorithm(WTFMove(pullAlgorithm))
    , m_cancelAlgorithm(WTFMove(cancelAlgorithm))
    , m_autoAllocateChunkSize(autoAllocateChunkSize)
    , m_underlyingSource(underlyingSource)
{
    m_pullAlgorithmWrapper =  [](auto& globalObject, auto& controller) {
        return getAlgorithmPromise(globalObject, controller.m_pullAlgorithm, controller.m_underlyingSource.getValue(), controller);
    };
    m_cancelAlgorithmWrapper = [](auto& globalObject, auto& controller, auto&& reason) {
        JSC::JSValue cancelReason = reason ? *reason : JSC::jsUndefined();
        return getAlgorithmPromise(globalObject, controller.m_cancelAlgorithm, controller.m_underlyingSource.getValue(), cancelReason);
    };
}

ReadableByteStreamController::ReadableByteStreamController(ReadableStream& stream, PullAlgorithm&& pullAlgorithm, CancelAlgorithm&& cancelAlgorithm, double highWaterMark, size_t autoAllocateChunkSize)
    : m_stream(stream)
    , m_strategyHWM(highWaterMark)
    , m_autoAllocateChunkSize(autoAllocateChunkSize)
    , m_pullAlgorithmWrapper(WTFMove(pullAlgorithm))
    , m_cancelAlgorithmWrapper(WTFMove(cancelAlgorithm))
{
}

ReadableByteStreamController::~ReadableByteStreamController() = default;

void ReadableByteStreamController::ref()
{
    m_stream->ref();
}

void ReadableByteStreamController::deref()
{
    m_stream->deref();
}

ReadableStream& ReadableByteStreamController::stream()
{
    return m_stream;
}

Ref<ReadableStream> ReadableByteStreamController::protectedStream()
{
    return stream();
}

// https://streams.spec.whatwg.org/#rbs-controller-byob-request
ReadableStreamBYOBRequest* ReadableByteStreamController::byobRequestForBindings() const
{
    return getByobRequest();
}

// https://streams.spec.whatwg.org/#rbs-controller-desired-size
std::optional<double> ReadableByteStreamController::desiredSize() const
{
    return getDesiredSize();
}

// https://streams.spec.whatwg.org/#rbs-controller-close
ExceptionOr<void> ReadableByteStreamController::closeForBindings(JSDOMGlobalObject& globalObject)
{
    if (m_closeRequested)
        return Exception { ExceptionCode::TypeError, "controller is closed"_s };

    if (protectedStream()->state() != ReadableStream::State::Readable)
        return Exception { ExceptionCode::TypeError, "controller's stream is not readable"_s };

    close(globalObject);
    return { };
}

// https://streams.spec.whatwg.org/#rbs-controller-enqueue
ExceptionOr<void> ReadableByteStreamController::enqueueForBindings(JSDOMGlobalObject& globalObject, JSC::ArrayBufferView& chunk)
{
    if (!chunk.byteLength())
        return Exception { ExceptionCode::TypeError, "chunk's size is 0"_s };

    RefPtr sharedBuffer = chunk.possiblySharedBuffer();
    if (!sharedBuffer || !sharedBuffer->byteLength())
        return Exception { ExceptionCode::TypeError, "chunk's buffer size is 0"_s };

    if (m_closeRequested)
        return Exception { ExceptionCode::TypeError, "controller is closed"_s };

    if (protectedStream()->state() != ReadableStream::State::Readable)
        return Exception { ExceptionCode::TypeError, "controller's stream is not readable"_s };

    return enqueue(globalObject, chunk);
}

// https://streams.spec.whatwg.org/#rbs-controller-error
ExceptionOr<void> ReadableByteStreamController::errorForBindings(JSDOMGlobalObject& globalObject, JSC::JSValue value)
{
    error(globalObject, value);
    return { };
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollergetbyobrequest
ReadableStreamBYOBRequest* ReadableByteStreamController::getByobRequest() const
{
    if (!m_byobRequest && !m_pendingPullIntos.isEmpty()) {
        auto& firstDescriptor = m_pendingPullIntos.first();
        auto view = JSC::Uint8Array::create(firstDescriptor.buffer.ptr(), firstDescriptor.byteOffset + firstDescriptor.bytesFilled, firstDescriptor.byteLength - firstDescriptor.bytesFilled);
        Ref byobRequest = ReadableStreamBYOBRequest::create();

        byobRequest->setController(const_cast<ReadableByteStreamController*>(this));
        byobRequest->setView(view.ptr());

        m_byobRequest = WTFMove(byobRequest);
    }

    return m_byobRequest.get();
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-get-desired-size
std::optional<double> ReadableByteStreamController::getDesiredSize() const
{
    Ref stream = m_stream.get();
    auto state = stream->state();
    if (state == ReadableStream::State::Errored)
        return { };
    if (state == ReadableStream::State::Closed)
        return 0;

    return m_strategyHWM - m_queueTotalSize;
}

ExceptionOr<void> ReadableByteStreamController::start(JSDOMGlobalObject& globalObject, UnderlyingSourceStartCallback* startAlgorithm)
{
    RefPtr<DOMPromise> startPromise;
    if (!startAlgorithm) {
        auto* promise = JSC::JSPromise::resolvedPromise(&globalObject, JSC::jsUndefined());
        startPromise = DOMPromise::create(globalObject, *promise);
    } else {
        auto startResult = startAlgorithm->invokeRethrowingException(m_underlyingSource.getValue(), *this);
        if (startResult.type() != CallbackResultType::Success) {
            return Exception { ExceptionCode::ExistingExceptionError };
        }
        Ref vm = globalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        auto* promise = JSC::JSPromise::resolvedPromise(&globalObject, startResult.releaseReturnValue());
        if (scope.exception())
            promise = JSC::JSPromise::rejectedPromise(&globalObject, JSC::jsUndefined());
        startPromise = DOMPromise::create(globalObject, *promise);
    }

    handleSourcePromise(*startPromise, [weakThis = WeakPtr { *this }](auto& globalObject, auto&& error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (error) {
            protectedThis->error(globalObject, *error);
            return;
        }

        protectedThis->didStart(globalObject);
    });
    return { };
}

void ReadableByteStreamController::didStart(JSDOMGlobalObject& globalObject)
{
    m_started = true;
    ASSERT(!m_pulling);
    ASSERT(!m_pullAgain);
    callPullIfNeeded(globalObject);
}

// Part of https://streams.spec.whatwg.org/#readablestream-close
void ReadableByteStreamController::closeAndRespondToPendingPullIntos(JSDOMGlobalObject& globalObject)
{
    close(globalObject);
    while (!m_pendingPullIntos.isEmpty())
        respond(globalObject, 0);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-close
void ReadableByteStreamController::close(JSDOMGlobalObject& globalObject)
{
    Ref stream = m_stream.get();

    if (m_closeRequested || stream->state() != ReadableStream::State::Readable)
        return;

    if (m_queueTotalSize) {
        m_closeRequested = true;
        return;
    }

    if (!m_pendingPullIntos.isEmpty()) {
        auto& pullInto = m_pendingPullIntos.first();
        if (pullInto.bytesFilled % pullInto.elementSize) {
            Ref vm = globalObject.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto error = createDOMException(&globalObject, ExceptionCode::TypeError, "controller has pending pull intos"_s);
            scope.assertNoExceptionExceptTermination();

            this->error(globalObject, error);
            throwException(&globalObject, scope, error);
            return;
        }
    }

    clearAlgorithms();
    stream->close();
}

// https://streams.spec.whatwg.org/#transfer-array-buffer
static ExceptionOr<Ref<JSC::ArrayBuffer>> transferArrayBuffer(JSC::VM& vm, JSC::ArrayBuffer& buffer)
{
    ASSERT(!buffer.isDetached());

    if (buffer.isWasmMemory())
        return Exception { ExceptionCode::TypeError, "transfer of buffer is not possible"_s };

    JSC::ArrayBufferContents contents;
    bool isOK = buffer.transferTo(vm, contents);
    if (!isOK)
        return Exception { ExceptionCode::TypeError, "transfer of buffer failed"_s };

    return ArrayBuffer::create(WTFMove(contents));
}

// https://streams.spec.whatwg.org/#readablestream-pull-from-bytes
size_t ReadableByteStreamController::pullFromBytes(JSDOMGlobalObject& globalObject, JSC::ArrayBuffer& buffer, size_t offset)
{
    ASSERT(offset < buffer.byteLength());
    auto available = buffer.byteLength() - offset;
    RefPtr request = getByobRequest();
    RefPtr currentView = request ? request->view() : nullptr;
    auto desiredSize = currentView ? currentView->byteLength() : available;

    auto pullSize = std::min(available, desiredSize);

    if (currentView) {
        memcpySpan(currentView->mutableSpan().subspan(0, pullSize), buffer.span().subspan(offset, pullSize));
        request->respond(globalObject, pullSize);
        return offset + pullSize;
    }

    enqueue(globalObject, buffer, offset, pullSize);
    return offset + pullSize;
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue
ExceptionOr<void> ReadableByteStreamController::enqueue(JSDOMGlobalObject& globalObject, JSC::ArrayBufferView& view)
{
    if (m_closeRequested || protectedStream()->state() != ReadableStream::State::Readable)
        return { };

    RefPtr buffer = view.possiblySharedBuffer();
    if (!buffer || buffer->isDetached())
        return Exception { ExceptionCode::TypeError, "view is detached"_s };

    return enqueue(globalObject, *buffer, view.byteOffset(), view.byteLength());
}

ExceptionOr<void> ReadableByteStreamController::enqueue(JSDOMGlobalObject& globalObject, JSC::ArrayBuffer& buffer)
{
    if (m_closeRequested || protectedStream()->state() != ReadableStream::State::Readable)
        return { };

    if (buffer.isDetached())
        return Exception { ExceptionCode::TypeError, "view is detached"_s };

    return enqueue(globalObject, buffer, 0, buffer.byteLength());
}

ExceptionOr<void> ReadableByteStreamController::enqueue(JSDOMGlobalObject& globalObject, JSC::ArrayBuffer& buffer, size_t byteOffset, size_t byteLength)
{
    ASSERT(!m_closeRequested && protectedStream()->state() == ReadableStream::State::Readable);
    ASSERT(!buffer.isDetached());

    Ref vm = globalObject.vm();

    auto transferredBufferOrException = transferArrayBuffer(vm, buffer);
    if (transferredBufferOrException.hasException())
        return transferredBufferOrException.releaseException();

    if (!m_pendingPullIntos.isEmpty()) {
        auto& firstPendingPullInto = m_pendingPullIntos.first();
        if (Ref { firstPendingPullInto.buffer }->isDetached())
            return Exception { ExceptionCode::TypeError, "pendingPullInto buffer is detached"_s };

        invalidateByobRequest();

        auto firstTransferredBufferOrException = transferArrayBuffer(vm, firstPendingPullInto.buffer.get());
        if (firstTransferredBufferOrException.hasException())
            return firstTransferredBufferOrException.releaseException();
        firstPendingPullInto.buffer = firstTransferredBufferOrException.releaseReturnValue();

        if (firstPendingPullInto.readerType == ReaderType::None)
            enqueueDetachedPullIntoToQueue(globalObject, firstPendingPullInto);
    }

    Ref stream = m_stream.get();
    if (stream->defaultReader()) {
        processReadRequestsUsingQueue(globalObject);
        if (!stream->getNumReadRequests()) {
            ASSERT(m_pendingPullIntos.isEmpty());
            enqueueChunkToQueue(transferredBufferOrException.releaseReturnValue(), byteOffset, byteLength);
        } else {
            ASSERT(m_queue.isEmpty());
            if (!m_pendingPullIntos.isEmpty()) {
                ASSERT(m_pendingPullIntos.first().readerType == ReaderType::Default);
                shiftPendingPullInto();
            }

            Ref transferredView = Uint8Array::create(transferredBufferOrException.releaseReturnValue(), byteOffset, byteLength);
            stream->fulfillReadRequest(globalObject, WTFMove(transferredView), false);
        }
    } else if (RefPtr byobReader = stream->byobReader()) {
        enqueueChunkToQueue(transferredBufferOrException.releaseReturnValue(), byteOffset, byteLength);
        auto filledPullIntos = processPullIntoDescriptorsUsingQueue();
        for (auto& pullInto : filledPullIntos)
            commitPullIntoDescriptor(globalObject, pullInto);
    } else {
        ASSERT(!protectedStream()->isLocked());
        enqueueChunkToQueue(transferredBufferOrException.releaseReturnValue(), byteOffset, byteLength);
    }

    callPullIfNeeded(globalObject);
    return { };
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerprocessreadrequestsusingqueue
void ReadableByteStreamController::processReadRequestsUsingQueue(JSDOMGlobalObject& globalObject)
{
    RefPtr reader = protectedStream()->defaultReader();

    ASSERT(reader);

    while (reader->getNumReadRequests()) {
        if (!m_queueTotalSize)
            return;

        auto readRequest = reader->takeFirstReadRequest();
        fillReadRequestFromQueue(globalObject, WTFMove(readRequest));
    }
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-invalidate-byob-request
void ReadableByteStreamController::invalidateByobRequest()
{
    RefPtr byobRequest = m_byobRequest;
    if (!byobRequest)
        return;

    byobRequest->setController(nullptr);
    byobRequest->setView(nullptr);
    m_byobRequest = nullptr;
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-process-pull-into-descriptors-using-queue
Vector<ReadableByteStreamController::PullIntoDescriptor> ReadableByteStreamController::processPullIntoDescriptorsUsingQueue()
{
    ASSERT(!m_closeRequested);
    Vector<PullIntoDescriptor> filledPullIntos;

    while (!m_pendingPullIntos.isEmpty()) {
        if (!m_queueTotalSize)
            break;

        auto& pullInto = m_pendingPullIntos.first();
        if (fillPullIntoDescriptorFromQueue(pullInto))
            filledPullIntos.append(shiftPendingPullInto());
    }
    return filledPullIntos;
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerenqueuedetachedpullintotoqueue
void ReadableByteStreamController::enqueueDetachedPullIntoToQueue(JSDOMGlobalObject& globalObject, PullIntoDescriptor& pullInto)
{
    ASSERT(pullInto.readerType == ReaderType::None);

    if (pullInto.bytesFilled > 0)
        enqueueClonedChunkToQueue(globalObject, pullInto.buffer.get(), pullInto.byteOffset, pullInto.bytesFilled);
    shiftPendingPullInto();
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-shift-pending-pull-into
ReadableByteStreamController::PullIntoDescriptor ReadableByteStreamController::shiftPendingPullInto()
{
    ASSERT(!m_byobRequest);
    return m_pendingPullIntos.takeFirst();
}

void ReadableByteStreamController::enqueueChunkToQueue(Ref<JSC::ArrayBuffer>&& buffer, size_t byteOffset, size_t byteLength)
{
    m_queue.append({ WTFMove(buffer), byteOffset, byteLength });
    m_queueTotalSize += byteLength;
}

static RefPtr<JSC::ArrayBuffer> cloneArrayBuffer(JSC::ArrayBuffer& buffer, size_t byteOffset, size_t byteLength)
{
    auto span = buffer.span().subspan(byteOffset, byteLength);
    return JSC::ArrayBuffer::tryCreate(span);
}

void ReadableByteStreamController::enqueueClonedChunkToQueue(JSDOMGlobalObject& globalObject, JSC::ArrayBuffer& buffer, size_t byteOffset, size_t byteLength)
{
    auto clone = cloneArrayBuffer(buffer, byteOffset, byteLength);
    if (!clone) {
        // FIXME: Provide a good error value.
        error(globalObject, JSC::jsUndefined());
        return;
    }
    enqueueChunkToQueue(clone.releaseNonNull(), 0, byteLength);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
void ReadableByteStreamController::callPullIfNeeded(JSDOMGlobalObject& globalObject)
{
    bool shouldPull = shouldCallPull();
    if (!shouldPull)
        return;

    if (m_pulling) {
        m_pullAgain = true;
        return;
    }

    ASSERT(!m_pullAgain);
    m_pulling = true;

    auto promise = m_pullAlgorithmWrapper(globalObject, *this);
    handleSourcePromise(promise, [weakThis = WeakPtr { *this }](auto& globalObject, auto&& error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (error) {
            protectedThis->error(globalObject, *error);
            return;
        }

        protectedThis->m_pulling = false;
        if (protectedThis->m_pullAgain) {
            protectedThis->m_pullAgain = false;
            protectedThis->callPullIfNeeded(globalObject);
        }
    });
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-should-call-pull
bool ReadableByteStreamController::shouldCallPull()
{
    if (protectedStream()->state() != ReadableStream::State::Readable)
        return false;

    if (m_closeRequested)
        return false;

    if (!m_started)
        return false;

    RefPtr defaultReader = protectedStream()->defaultReader();
    if (defaultReader && defaultReader->getNumReadRequests() > 0)
        return true;

    RefPtr byobReader = protectedStream()->byobReader();
    if (byobReader && byobReader->readIntoRequestsSize() > 0)
        return true;

    return getDesiredSize() > 0;
}

static void copyDataBlockBytes(JSC::ArrayBuffer& destination, size_t destinationStart, JSC::ArrayBuffer& source, size_t sourceOffset, size_t bytesToCopy)
{
    memcpySpan(destination.mutableSpan().subspan(destinationStart, bytesToCopy), source.span().subspan(sourceOffset, bytesToCopy));
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-pull-into-descriptor-from-queue
bool ReadableByteStreamController::fillPullIntoDescriptorFromQueue(PullIntoDescriptor& pullInto)
{
    size_t maxBytesToCopy = std::min(m_queueTotalSize, pullInto.byteLength - pullInto.bytesFilled);
    size_t maxBytesFilled = pullInto.bytesFilled + maxBytesToCopy;
    size_t totalBytesToCopyRemaining = maxBytesToCopy;
    bool isReady = false;

    ASSERT(pullInto.bytesFilled < pullInto.minimumFill);
    size_t remainderBytes = maxBytesFilled % pullInto.elementSize;
    size_t maxAlignedBytes = maxBytesFilled - remainderBytes;

    if (maxAlignedBytes >= pullInto.minimumFill) {
        totalBytesToCopyRemaining = maxAlignedBytes - pullInto.bytesFilled;
        isReady = true;
    }

    while (totalBytesToCopyRemaining > 0) {
        auto& headOfQueue = m_queue.first();
        size_t bytesToCopy = std::min(totalBytesToCopyRemaining, headOfQueue.byteLength);
        size_t destStart = pullInto.byteOffset + pullInto.bytesFilled;
        copyDataBlockBytes(pullInto.buffer.get(), destStart, headOfQueue.buffer.get(), headOfQueue.byteOffset, bytesToCopy);
        if (headOfQueue.byteLength == bytesToCopy)
            m_queue.takeFirst();
        else {
            headOfQueue.byteOffset = headOfQueue.byteOffset + bytesToCopy;
            headOfQueue.byteLength = headOfQueue.byteLength - bytesToCopy;
        }
        m_queueTotalSize -= bytesToCopy;
        fillHeadPullIntoDescriptor(bytesToCopy, pullInto);
        totalBytesToCopyRemaining -= bytesToCopy;
    }
    if (!isReady) {
        ASSERT(!m_queueTotalSize);
        ASSERT(pullInto.bytesFilled > 0.);
        ASSERT(pullInto.bytesFilled < pullInto.minimumFill);
    }
    return isReady;
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-head-pull-into-descriptor
void ReadableByteStreamController::fillHeadPullIntoDescriptor(size_t size, PullIntoDescriptor& pullInto)
{
    ASSERT(m_pendingPullIntos.isEmpty() || &pullInto == &m_pendingPullIntos.first());
    ASSERT(!m_byobRequest);
    pullInto.bytesFilled += size;
}

static Ref<JSC::ArrayBufferView> createTypedBuffer(JSC::TypedArrayType type, Ref<JSC::ArrayBuffer>&& buffer, size_t byteOffset, size_t size)
{
    switch (type) {
    case JSC::TypedArrayType::NotTypedArray:
    case JSC::TypedArrayType::TypeDataView:
        return JSC::DataView::create(WTFMove(buffer), byteOffset, size);
#define CREATE_TYPED_ARRAY(name) \
    case JSC::TypedArrayType::Type##name: \
        return JSC::name##Array::create(WTFMove(buffer), byteOffset, size);
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(CREATE_TYPED_ARRAY)
#undef CREATE_TYPED_ARRAY
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-convert-pull-into-descriptor
RefPtr<JSC::ArrayBufferView> ReadableByteStreamController::convertPullIntoDescriptor(JSC::VM& vm, PullIntoDescriptor& pullInto)
{
    auto bytesFilled = pullInto.bytesFilled;
    auto elementSize = pullInto.elementSize;
    ASSERT(bytesFilled <= pullInto.byteLength);
    ASSERT(!(bytesFilled % elementSize));

    auto buffer = transferArrayBuffer(vm, pullInto.buffer);
    if (buffer.hasException())
        return nullptr;

    return createTypedBuffer(pullInto.viewConstructor, buffer.releaseReturnValue(), pullInto.byteOffset, bytesFilled / elementSize);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-error
void ReadableByteStreamController::error(JSDOMGlobalObject& globalObject, JSC::JSValue value)
{
    Ref stream = m_stream.get();
    if (stream->state() != ReadableStream::State::Readable)
        return;

    clearPendingPullIntos();

    m_queue = { };
    m_queueTotalSize = 0;

    clearAlgorithms();
    stream->error(globalObject, value);
}

void ReadableByteStreamController::error(JSDOMGlobalObject& globalObject, const Exception& exception)
{
    auto& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto value = createDOMException(&globalObject, exception.code(), exception.message());

    if (scope.exception()) [[unlikely]] {
        ASSERT(vm.hasPendingTerminationException());
        return;
    }

    error(globalObject, value);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-pending-pull-intos
void ReadableByteStreamController::clearPendingPullIntos()
{
    invalidateByobRequest();
    m_pendingPullIntos = { };
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-algorithms
void ReadableByteStreamController::clearAlgorithms()
{
    m_pullAlgorithm = nullptr;
    m_cancelAlgorithm = nullptr;
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-pull-into
void ReadableByteStreamController::pullInto(JSDOMGlobalObject& globalObject, JSC::ArrayBufferView& view, size_t min, Ref<ReadableStreamReadIntoRequest>&& readIntoRequest)
{
    Ref stream = m_stream.get();
    size_t elementSize = 1;
    auto viewType = view.getType();
    if (viewType != JSC::TypedArrayType::TypeDataView)
        elementSize = JSC::elementSize(view.getType());

    auto minimumFill = min * elementSize;
    ASSERT(minimumFill <= view.byteLength());
    ASSERT(!(minimumFill % elementSize));

    auto byteOffset = view.byteOffset();
    auto byteLength = view.byteLength();
    if (view.isDetached()) {
        readIntoRequest->runErrorSteps(Exception { ExceptionCode::TypeError, "view is detached"_s });
        return;
    }

    Ref vm = globalObject.vm();
    auto bufferResultOrException = transferArrayBuffer(vm.get(), *view.possiblySharedBuffer());
    if (bufferResultOrException.hasException()) {
        readIntoRequest->runErrorSteps(bufferResultOrException.releaseException());
        return;
    }

    Ref buffer = bufferResultOrException.releaseReturnValue();

    auto bufferByteLength = buffer->byteLength();
    PullIntoDescriptor pullIntoDescriptor { WTFMove(buffer), bufferByteLength, byteOffset, byteLength, 0, minimumFill, elementSize, viewType, ReaderType::Byob };
    if (!m_pendingPullIntos.isEmpty()) {
        m_pendingPullIntos.append(WTFMove(pullIntoDescriptor));
        stream->addReadIntoRequest(WTFMove(readIntoRequest));
        return;
    }

    if (stream->state() == ReadableStream::State::Closed) {
        Ref emptyView = createTypedBuffer(pullIntoDescriptor.viewConstructor, WTFMove(pullIntoDescriptor.buffer), pullIntoDescriptor.byteOffset, 0);
        auto chunk = toJS<IDLArrayBufferView>(globalObject, globalObject, WTFMove(emptyView));
        readIntoRequest->runCloseSteps(chunk);
        return;
    }

    if (m_queueTotalSize > 0) {
        if (fillPullIntoDescriptorFromQueue(pullIntoDescriptor)) {
            auto filledView = convertPullIntoDescriptor(vm, pullIntoDescriptor);
            handleQueueDrain(globalObject);

            auto chunk = toJS<IDLNullable<IDLArrayBufferView>>(globalObject, globalObject, WTFMove(filledView));
            readIntoRequest->runChunkSteps(chunk);
            return;
        }
        if (m_closeRequested) {
            JSC::JSValue e = JSC::createTypeError(&globalObject, "close is requested"_s);
            error(globalObject, e);
            readIntoRequest->runErrorSteps(e);
            return;
        }
    }

    m_pendingPullIntos.append(WTFMove(pullIntoDescriptor));
    stream->addReadIntoRequest(WTFMove(readIntoRequest));
    callPullIfNeeded(globalObject);
}

// https://streams.spec.whatwg.org/#rbs-controller-private-cancel
void ReadableByteStreamController::runCancelSteps(JSDOMGlobalObject& globalObject, JSC::JSValue reason, Function<void(std::optional<JSC::JSValue>&&)>&& callback)
{
    clearPendingPullIntos();

    m_queue = { };
    m_queueTotalSize = 0;

    auto promise = m_cancelAlgorithmWrapper(globalObject, *this, reason);
    handleSourcePromise(promise, [callback = WTFMove(callback)](auto&, auto&& reason) mutable {
        callback(WTFMove(reason));
    });
}

// https://streams.spec.whatwg.org/#rbs-controller-private-pull
void ReadableByteStreamController::runPullSteps(JSDOMGlobalObject& globalObject, Ref<ReadableStreamReadRequest>&& readRequest)
{
    Ref stream = m_stream.get();
    ASSERT(stream->defaultReader());

    if (m_queueTotalSize) {
        ASSERT(!stream->getNumReadRequests());
        fillReadRequestFromQueue(globalObject, WTFMove(readRequest));
        return;
    }

    if (auto autoAllocateChunkSize = m_autoAllocateChunkSize) {
        auto buffer = JSC::ArrayBuffer::create(autoAllocateChunkSize, 1);
        m_pendingPullIntos.append({ WTFMove(buffer), autoAllocateChunkSize, 0, autoAllocateChunkSize, 0, 1, 1, JSC::TypedArrayType::TypeUint8, ReaderType::Default });
    }
    stream->addReadRequest(WTFMove(readRequest));
    callPullIfNeeded(globalObject);
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontroller-releasesteps
void ReadableByteStreamController::runReleaseSteps()
{
    if (!m_pendingPullIntos.isEmpty()) {
        m_pendingPullIntos.first().readerType = ReaderType::None;
        while (m_pendingPullIntos.size() > 1)
            m_pendingPullIntos.removeLast();
    }
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerfillreadrequestfromqueue
void ReadableByteStreamController::fillReadRequestFromQueue(JSDOMGlobalObject& globalObject, Ref<ReadableStreamReadRequest>&& readRequest)
{
    ASSERT(m_queueTotalSize);
    auto entry = m_queue.takeFirst();
    m_queueTotalSize -= entry.byteLength;

    handleQueueDrain(globalObject);

    Ref view = Uint8Array::create(WTFMove(entry.buffer), entry.byteOffset, entry.byteLength);
    auto chunk = toJS<IDLArrayBufferView>(globalObject, globalObject, WTFMove(view));
    readRequest->runChunkSteps(chunk);
}

void ReadableByteStreamController::storeError(JSDOMGlobalObject& globalObject, JSC::JSValue error)
{
    Ref vm = globalObject.vm();
    auto thisValue = toJS(&globalObject, &globalObject, *this);
    m_storedError.set(vm.get(), thisValue.getObject(), error);
}

JSC::JSValue ReadableByteStreamController::storedError() const
{
    return m_storedError.getValue();
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond
ExceptionOr<void> ReadableByteStreamController::respond(JSDOMGlobalObject& globalObject, size_t bytesWritten)
{
    ASSERT(!m_pendingPullIntos.isEmpty());
    auto& firstDescriptor = m_pendingPullIntos.first();
    auto state = protectedStream()->state();
    if (state == ReadableStream::State::Closed) {
        if (bytesWritten > 0)
            return Exception { ExceptionCode::TypeError, "stream is closed"_s };
    } else {
        ASSERT(state == ReadableStream::State::Readable);
        if (!bytesWritten)
            return Exception { ExceptionCode::TypeError, "bytesWritten is 0"_s };
        if (firstDescriptor.bytesFilled + bytesWritten > firstDescriptor.byteLength)
            return Exception { ExceptionCode::RangeError, "bytesWritten is too big"_s };
    }

    Ref vm = globalObject.vm();
    auto transferredBufferOrException = transferArrayBuffer(vm.get(), firstDescriptor.buffer.get());
    if (transferredBufferOrException.hasException())
        return transferredBufferOrException.releaseException();

    firstDescriptor.buffer = transferredBufferOrException.releaseReturnValue();

    respondInternal(globalObject, bytesWritten);
    return { };
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-with-new-view
ExceptionOr<void> ReadableByteStreamController::respondWithNewView(JSDOMGlobalObject& globalObject, JSC::ArrayBufferView& view)
{
    ASSERT(!m_pendingPullIntos.isEmpty());
    ASSERT(!view.isDetached());

    auto& firstDescriptor = m_pendingPullIntos.first();
    auto state = protectedStream()->state();
    if (state == ReadableStream::State::Closed) {
        if (!!view.byteLength())
            return Exception { ExceptionCode::TypeError, "stream is closed"_s };
    } else {
        ASSERT(state == ReadableStream::State::Readable);
        if (!view.byteLength())
            return Exception { ExceptionCode::TypeError, "bytesWritten is 0"_s };
    }

    if (firstDescriptor.byteOffset + firstDescriptor.bytesFilled != view.byteOffset())
        return Exception { ExceptionCode::RangeError, "Wrong byte offset"_s };

    RefPtr viewedArrayBuffer = view.possiblySharedBuffer();
    auto viewedArrayBufferByteLength = viewedArrayBuffer ? viewedArrayBuffer->byteLength() : 0;
    if (firstDescriptor.bufferByteLength != viewedArrayBufferByteLength)
        return Exception { ExceptionCode::RangeError, "Wrong view buffer byte length"_s };

    if (firstDescriptor.bytesFilled + view.byteLength() > firstDescriptor.byteLength)
        return Exception { ExceptionCode::RangeError, "Wrong byte length"_s };

    auto viewByteLength = view.byteLength();

    Ref vm = globalObject.vm();
    auto transferredBufferOrException = transferArrayBuffer(vm, *view.possiblySharedBuffer());
    if (transferredBufferOrException.hasException())
        return transferredBufferOrException.releaseException();
    firstDescriptor.buffer = transferredBufferOrException.releaseReturnValue();

    respondInternal(globalObject, viewByteLength);
    return { };
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-internal
void ReadableByteStreamController::respondInternal(JSDOMGlobalObject& globalObject, size_t bytesWritten)
{
    auto& firstDescriptor = m_pendingPullIntos.first();
    ASSERT(!firstDescriptor.buffer->isDetached());
    invalidateByobRequest();

    auto state = protectedStream()->state();
    if (state == ReadableStream::State::Closed) {
        ASSERT(!bytesWritten);
        respondInClosedState(globalObject, firstDescriptor);
    } else {
        ASSERT(state == ReadableStream::State::Readable);
        ASSERT(bytesWritten > 0);
        respondInReadableState(globalObject, bytesWritten, firstDescriptor);
    }
    callPullIfNeeded(globalObject);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-closed-state
void ReadableByteStreamController::respondInClosedState(JSDOMGlobalObject& globalObject, PullIntoDescriptor& firstDescriptor)
{
    ASSERT(!(firstDescriptor.bytesFilled % firstDescriptor.elementSize));

    if (firstDescriptor.readerType == ReaderType::None)
        shiftPendingPullInto();

    Ref stream = m_stream.get();
    if (RefPtr byobReader = stream->byobReader()) {
        while (stream->getNumReadIntoRequests() > 0) {
            auto pullIntoDescriptor = shiftPendingPullInto();
            commitPullIntoDescriptor(globalObject, pullIntoDescriptor);
        }
    }
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-readable-state
void ReadableByteStreamController::respondInReadableState(JSDOMGlobalObject& globalObject, size_t bytesWritten, PullIntoDescriptor& pullIntoDescriptor)
{
    ASSERT(pullIntoDescriptor.bytesFilled + bytesWritten <= pullIntoDescriptor.byteLength);
    fillHeadPullIntoDescriptor(bytesWritten, pullIntoDescriptor);

    if (pullIntoDescriptor.readerType == ReaderType::None) {
        enqueueDetachedPullIntoToQueue(globalObject, pullIntoDescriptor);
        auto filledPullIntos = processPullIntoDescriptorsUsingQueue();
        for (auto& pullInto : filledPullIntos)
            commitPullIntoDescriptor(globalObject, pullInto);
        return;
    }
    if (pullIntoDescriptor.bytesFilled < pullIntoDescriptor.minimumFill)
        return;

    auto pullInto = shiftPendingPullInto();

    auto remainderSize = pullInto.bytesFilled % pullInto.elementSize;
    if (remainderSize > 0) {
        auto end = pullInto.byteOffset + pullInto.bytesFilled;
        enqueueClonedChunkToQueue(globalObject, pullInto.buffer.get(), end - remainderSize, remainderSize);
    }

    pullInto.bytesFilled -= remainderSize;
    commitPullIntoDescriptor(globalObject, pullInto);
    auto filledPullIntos = processPullIntoDescriptorsUsingQueue();
    for (auto& pullInto : filledPullIntos)
        commitPullIntoDescriptor(globalObject, pullInto);
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-commit-pull-into-descriptor
void ReadableByteStreamController::commitPullIntoDescriptor(JSDOMGlobalObject& globalObject, PullIntoDescriptor& pullIntoDescriptor)
{
    Ref stream = m_stream.get();
    auto state = stream->state();

    ASSERT(stream->state() != ReadableStream::State::Errored);
    ASSERT(pullIntoDescriptor.readerType != ReaderType::None);

    bool done = false;

    if (state == ReadableStream::State::Closed) {
        ASSERT(!(pullIntoDescriptor.bytesFilled % pullIntoDescriptor.elementSize));
        done = true;
    }

    Ref vm = globalObject.vm();
    RefPtr filledView = convertPullIntoDescriptor(vm.get(), pullIntoDescriptor);
    if (pullIntoDescriptor.readerType == ReaderType::Default)
        stream->fulfillReadRequest(globalObject, WTFMove(filledView), done);
    else {
        ASSERT(pullIntoDescriptor.readerType == ReaderType::Byob);
        stream->fulfillReadIntoRequest(globalObject, WTFMove(filledView), done);
    }
}

// https://streams.spec.whatwg.org/#readable-byte-stream-controller-handle-queue-drain
void ReadableByteStreamController::handleQueueDrain(JSDOMGlobalObject& globalObject)
{
    ASSERT(protectedStream()->state() == ReadableStream::State::Readable);

    if (!m_queueTotalSize && m_closeRequested) {
        clearAlgorithms();
        protectedStream()->close();
    } else
        callPullIfNeeded(globalObject);
}

void ReadableByteStreamController::handleSourcePromise(DOMPromise& algorithmPromise, Callback&& callback)
{
    algorithmPromise.whenSettled([promise = Ref { algorithmPromise }, callback = WTFMove(callback)]() mutable {
        auto* globalObject = promise->globalObject();
        if (!globalObject || promise->isSuspended())
            return;

        switch (promise->status()) {
        case DOMPromise::Status::Fulfilled:
            callback(*globalObject, { });
            break;
        case DOMPromise::Status::Rejected:
            callback(*globalObject, promise->result());
            break;
        case DOMPromise::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

template<typename Visitor>
void ReadableByteStreamController::visitAdditionalChildren(Visitor& visitor)
{
    SUPPRESS_UNCOUNTED_ARG m_stream->visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(ReadableByteStreamController);

template<typename Visitor>
void JSReadableByteStreamController::visitAdditionalChildren(Visitor& visitor)
{
    // Do not ref `wrapped()` here since this function may get called on the GC thread.
    SUPPRESS_UNCOUNTED_ARG wrapped().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSReadableByteStreamController);

} // namespace WebCore
