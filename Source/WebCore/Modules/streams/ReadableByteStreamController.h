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

#pragma once

#include "ExceptionOr.h"
#include "JSValueInWrappedObject.h"
#include "ReadableStreamReadRequest.h"
#include <wtf/Deque.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
class JSValue;
class VM;
}

namespace WebCore {

class DOMPromise;
class DeferredPromise;
class JSDOMGlobalObject;
class ReadableStream;
class ReadableStreamBYOBRequest;
class ReadableStreamReadRequest;
class UnderlyingSourceCancelCallback;
class UnderlyingSourcePullCallback;
class UnderlyingSourceStartCallback;

class ReadableByteStreamController : public CanMakeWeakPtr<ReadableByteStreamController> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ReadableByteStreamController);
public:
    ~ReadableByteStreamController();

    ReadableStreamBYOBRequest* byobRequestForBindings() const;
    std::optional<double> desiredSize() const;
    ExceptionOr<void> closeForBindings(JSDOMGlobalObject&);
    ExceptionOr<void> enqueueForBindings(JSDOMGlobalObject&, JSC::ArrayBufferView&);
    ExceptionOr<void> errorForBindings(JSDOMGlobalObject&, JSC::JSValue);

    ExceptionOr<void> start(JSDOMGlobalObject&, UnderlyingSourceStartCallback*);

    void respondPendingPullIntosOnClose(JSDOMGlobalObject&);

    ReadableStream& stream();
    Ref<ReadableStream> protectedStream();

    void pullInto(JSDOMGlobalObject&, JSC::ArrayBufferView&, size_t, Ref<ReadableStreamReadIntoRequest>&&);

    void runCancelSteps(JSDOMGlobalObject&, JSC::JSValue, Function<void(std::optional<JSC::JSValue>&&)>&&);
    void runPullSteps(JSDOMGlobalObject&, Ref<ReadableStreamReadRequest>&&);
    void runReleaseSteps();

    void storeError(JSDOMGlobalObject&, JSC::JSValue);
    JSC::JSValue storedError() const;

    ExceptionOr<void> respond(JSDOMGlobalObject&, size_t);
    ExceptionOr<void> respondWithNewView(JSDOMGlobalObject&, JSC::ArrayBufferView&);

    ReadableStreamBYOBRequest* getByobRequest() const;

    bool hasPendingPullIntos() const { return !m_pendingPullIntos.isEmpty(); }

    void ref();
    void deref();

    void error(JSDOMGlobalObject&, const Exception&);
    void error(JSDOMGlobalObject&, JSC::JSValue);
    void close(JSDOMGlobalObject&);
    void closeAndRespondToPendingPullIntos(JSDOMGlobalObject&);
    size_t pullFromBytes(JSDOMGlobalObject&, JSC::ArrayBuffer&, size_t offset);
    ExceptionOr<void> enqueue(JSDOMGlobalObject&, JSC::ArrayBufferView&);
    ExceptionOr<void> enqueue(JSDOMGlobalObject&, JSC::ArrayBuffer&);

    bool isPulling() const { return m_pulling; }

    template<typename Visitor> void visitAdditionalChildren(Visitor&);

    JSValueInWrappedObject& underlyingSourceConcurrently() { return m_underlyingSource; }
    JSValueInWrappedObject& storedErrorConcurrently() { return m_storedError; }

    using PullAlgorithm = Function<Ref<DOMPromise>(JSDOMGlobalObject&, ReadableByteStreamController&)>;
    using CancelAlgorithm = Function<Ref<DOMPromise>(JSDOMGlobalObject&, ReadableByteStreamController&, std::optional<JSC::JSValue>&&)>;

private:
    friend ReadableStream;
    ReadableByteStreamController(ReadableStream&, JSC::JSValue, RefPtr<UnderlyingSourcePullCallback>&&, RefPtr<UnderlyingSourceCancelCallback>&&, double highWaterMark, size_t autoAllocateChunkSize);

    ExceptionOr<void> enqueue(JSDOMGlobalObject&, JSC::ArrayBuffer&, size_t byteOffset, size_t byteLength);

    using Callback = Function<void(JSDOMGlobalObject&, std::optional<JSC::JSValue>&&)>;
    ReadableByteStreamController(ReadableStream&, PullAlgorithm&&, CancelAlgorithm&&, double highWaterMark, size_t autoAllocateChunkSize);

    enum ReaderType : uint8_t { None, Default, Byob };

    struct PullIntoDescriptor {
        Ref<JSC::ArrayBuffer> buffer;
        size_t bufferByteLength { 0 };
        size_t byteOffset { 0 };
        size_t byteLength { 0 };
        size_t bytesFilled { 0 };
        size_t minimumFill { 0 };
        size_t elementSize { 0 };
        JSC::TypedArrayType viewConstructor;
        ReaderType readerType;
    };

    struct Entry {
        Ref<JSC::ArrayBuffer> buffer;
        size_t byteOffset { 0 };
        size_t byteLength { 0 };
    };

    std::optional<double> getDesiredSize() const;
    void didStart(JSDOMGlobalObject&);

    void invalidateByobRequest();
    Vector<PullIntoDescriptor> processPullIntoDescriptorsUsingQueue();
    void enqueueDetachedPullIntoToQueue(JSDOMGlobalObject&, PullIntoDescriptor&);
    PullIntoDescriptor shiftPendingPullInto();
    void enqueueChunkToQueue(Ref<JSC::ArrayBuffer>&&, size_t byteOffset, size_t byteLength);
    void enqueueClonedChunkToQueue(JSDOMGlobalObject&, JSC::ArrayBuffer&, size_t byteOffset, size_t byteLength);
    void callPullIfNeeded(JSDOMGlobalObject&);
    bool shouldCallPull();
    bool fillPullIntoDescriptorFromQueue(PullIntoDescriptor&);
    RefPtr<JSC::ArrayBufferView> convertPullIntoDescriptor(JSC::VM&, PullIntoDescriptor&);
    void fillHeadPullIntoDescriptor(size_t, PullIntoDescriptor&);
    void commitPullIntoDescriptor(JSDOMGlobalObject&, PullIntoDescriptor&);

    void clearAlgorithms();
    void clearPendingPullIntos();

    void respondInternal(JSDOMGlobalObject&, size_t);
    void respondInClosedState(JSDOMGlobalObject&, PullIntoDescriptor&);
    void respondInReadableState(JSDOMGlobalObject&, size_t, PullIntoDescriptor&);

    void processReadRequestsUsingQueue(JSDOMGlobalObject&);
    void fillReadRequestFromQueue(JSDOMGlobalObject&, Ref<ReadableStreamReadRequest>&&);
    void handleQueueDrain(JSDOMGlobalObject&);

    static void handleSourcePromise(DOMPromise&, Callback&&);

    WeakRef<ReadableStream> m_stream;
    bool m_pullAgain { false };
    bool m_pulling { false };
    mutable RefPtr<ReadableStreamBYOBRequest> m_byobRequest;
    bool m_closeRequested { false };
    bool m_started { false };
    double m_strategyHWM { 0 };
    RefPtr<UnderlyingSourcePullCallback> m_pullAlgorithm;
    RefPtr<UnderlyingSourceCancelCallback> m_cancelAlgorithm;
    size_t m_autoAllocateChunkSize { 0 };
    Deque<PullIntoDescriptor> m_pendingPullIntos;
    Deque<Entry> m_queue;
    size_t m_queueTotalSize { 0 };

    JSValueInWrappedObject m_underlyingSource;
    JSValueInWrappedObject m_storedError;

    PullAlgorithm m_pullAlgorithmWrapper;
    CancelAlgorithm m_cancelAlgorithmWrapper;
};

} // namespace WebCore
