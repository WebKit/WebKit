/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserver.h"
#include "ExceptionOr.h"
#include "InternalReadableStream.h"
#include "JSValueInWrappedObject.h"
#include "ReadableByteStreamController.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/AbstractRefCounted.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class AbstractSlotVisitor;
}

namespace WebCore {

class DOMPromise;
class DeferredPromise;
class InternalReadableStream;
class JSDOMGlobalObject;
class MessagePort;
class ReadableStreamBYOBReader;
class ReadableStreamDefaultReader;
class ReadableStreamReadRequest;
class ReadableStreamSource;
class WritableStream;

struct StreamPipeOptions;
struct UnderlyingSource;

using ReadableStreamReader = Variant<RefPtr<ReadableStreamDefaultReader>, RefPtr<ReadableStreamBYOBReader>>;

struct DetachedReadableStream {
    Ref<MessagePort> readableStreamPort;
};

class ReadableStream : public RefCounted<ReadableStream>, public ContextDestructionObserver {
public:
    enum class ReaderMode { Byob };
    struct GetReaderOptions {
        std::optional<ReaderMode> mode;
    };
    struct WritablePair {
        Ref<ReadableStream> readable;
        Ref<WritableStream> writable;
    };
    struct IteratorOptions {
        bool preventCancel { false };
    };

    static ExceptionOr<Ref<ReadableStream>> create(JSDOMGlobalObject&, JSC::Strong<JSC::JSObject>&&, JSC::Strong<JSC::JSObject>&&);
    static ExceptionOr<Ref<ReadableStream>> create(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&, std::optional<double> = { });
    static ExceptionOr<Ref<ReadableStream>> createFromByteUnderlyingSource(JSDOMGlobalObject&, JSC::JSValue underlyingSource, UnderlyingSource&&, double highWaterMark);
    static Ref<ReadableStream> create(Ref<InternalReadableStream>&&);

    static ExceptionOr<Ref<ReadableStream>> from(JSDOMGlobalObject&, JSC::JSValue);

    virtual ~ReadableStream();

    ExceptionOr<DetachedReadableStream> runTransferSteps(JSDOMGlobalObject&);
    static ExceptionOr<Ref<ReadableStream>> runTransferReceivingSteps(JSDOMGlobalObject&, DetachedReadableStream&&);

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    Ref<DOMPromise> cancelForBindings(JSDOMGlobalObject&, JSC::JSValue);
    ExceptionOr<ReadableStreamReader> getReader(JSDOMGlobalObject&, const GetReaderOptions&);
    ExceptionOr<Vector<Ref<ReadableStream>>> tee(JSDOMGlobalObject&, bool shouldClone = false);

    using State = InternalReadableStream::State;
    State state() const;

    void lock();
    bool isLocked() const;
    bool canTransfer() const;
    WEBCORE_EXPORT bool isDisturbed() const;

    Ref<DOMPromise> cancel(JSDOMGlobalObject&, JSC::JSValue);
    void cancel(Exception&&);

    InternalReadableStream* internalReadableStream() { return m_internalReadableStream.get(); }

    void setDefaultReader(ReadableStreamDefaultReader*);
    ReadableStreamDefaultReader* defaultReader();

    bool hasByteStreamController() { return !!m_controller; }
    ReadableByteStreamController* controller() { return m_controller.get(); }
    RefPtr<ReadableByteStreamController> protectedController() { return m_controller.get(); }

    void setByobReader(ReadableStreamBYOBReader*);
    ReadableStreamBYOBReader* byobReader();
    void fulfillReadIntoRequest(JSDOMGlobalObject&, RefPtr<JSC::ArrayBufferView>&&, bool done);

    void fulfillReadRequest(JSDOMGlobalObject&, RefPtr<JSC::ArrayBufferView>&&, bool done);

    void markAsDisturbed() { m_disturbed = true; }

    void close();
    JSC::JSValue storedError(JSDOMGlobalObject&) const;

    size_t getNumReadRequests() const;
    void addReadRequest(Ref<ReadableStreamReadRequest>&&);

    size_t getNumReadIntoRequests() const;
    void addReadIntoRequest(Ref<ReadableStreamReadIntoRequest>&&);

    void error(JSDOMGlobalObject&, JSC::JSValue);
    void pipeTo(JSDOMGlobalObject&, WritableStream&, StreamPipeOptions&&, Ref<DeferredPromise>&&);
    ExceptionOr<Ref<ReadableStream>> pipeThrough(JSDOMGlobalObject&, WritablePair&&, StreamPipeOptions&&);

    bool isReachableFromOpaqueRoots() const { return m_isSourceReachableFromOpaqueRoot && m_state == State::Readable; }
    enum class VisitTeedChildren : bool { No, Yes };
    void visitAdditionalChildren(JSC::AbstractSlotVisitor&, VisitTeedChildren = VisitTeedChildren::No);
    void setTeedBranches(ReadableStream&, ReadableStream&);
    void setSourceTeedStream(ReadableStream&);

    class DependencyToVisit : public AbstractRefCounted {
    public:
        virtual ~DependencyToVisit() = default;
        virtual void visit(JSC::AbstractSlotVisitor&) = 0;
    };
    enum class StartSynchronously : bool { No, Yes };
    enum class IsSourceReachableFromOpaqueRoot : bool { No, Yes };
    struct ByteStreamOptions {
        RefPtr<DependencyToVisit> dependencyToVisit { };
        double highwaterMark { 0 };
        StartSynchronously startSynchronously { StartSynchronously::No };
        IsSourceReachableFromOpaqueRoot isSourceReachableFromOpaqueRoot { IsSourceReachableFromOpaqueRoot::No };
    };
    static Ref<ReadableStream> createReadableByteStream(JSDOMGlobalObject&, ReadableByteStreamController::PullAlgorithm&&, ReadableByteStreamController::CancelAlgorithm&&, ByteStreamOptions&&);

    enum class Type : bool {
        Default,
        WebTransport
    };
    virtual Type type() const { return Type::Default; }

    JSDOMGlobalObject* globalObject();

    class Iterator : public RefCountedAndCanMakeWeakPtr<Iterator> {
    public:
        static Ref<Iterator> create(Ref<ReadableStreamDefaultReader>&&, bool preventCancel);
        ~Iterator();

        Ref<DOMPromise> next(JSDOMGlobalObject&);
        bool isFinished() const;
        Ref<DOMPromise> returnSteps(JSDOMGlobalObject&, JSC::JSValue);

    private:
        Iterator(Ref<ReadableStreamDefaultReader>&&, bool preventCancel);

        const Ref<ReadableStreamDefaultReader> m_reader;
        bool m_preventCancel { false };
    };

    ExceptionOr<Ref<Iterator>> createIterator(ScriptExecutionContext*, std::optional<IteratorOptions>&&);

protected:
    static ExceptionOr<Ref<ReadableStream>> createFromJSValues(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue, std::optional<double>);
    static ExceptionOr<Ref<InternalReadableStream>> createInternalReadableStream(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    explicit ReadableStream(ScriptExecutionContext*, RefPtr<InternalReadableStream>&& = { }, RefPtr<DependencyToVisit>&& = { }, IsSourceReachableFromOpaqueRoot = IsSourceReachableFromOpaqueRoot::No);

private:
    ExceptionOr<void> setupReadableByteStreamControllerFromUnderlyingSource(JSDOMGlobalObject&, JSC::JSValue, UnderlyingSource&&, double);
    void setupReadableByteStreamController(JSDOMGlobalObject&, ReadableByteStreamController::PullAlgorithm&&, ReadableByteStreamController::CancelAlgorithm&&, double, StartSynchronously);

    bool isPulling() const;
    void teedBranchIsDestroyed(ReadableStream&);

    const bool m_isSourceReachableFromOpaqueRoot { false };
    bool m_disturbed { false };
    WeakPtr<ReadableStreamDefaultReader> m_defaultReader;
    WeakPtr<ReadableStreamBYOBReader> m_byobReader;
    State m_state { State::Readable };

    const std::unique_ptr<ReadableByteStreamController> m_controller;
    const RefPtr<InternalReadableStream> m_internalReadableStream;

    const RefPtr<DependencyToVisit> m_dependencyToVisit;
    Lock m_gcLock;
    WeakPtr<ReadableStream> m_teedBranch0ForGC WTF_GUARDED_BY_LOCK(m_gcLock);
    WeakPtr<ReadableStream> m_teedBranch1ForGC WTF_GUARDED_BY_LOCK(m_gcLock);
    WeakPtr<ReadableStream> m_sourceTeedStream;
};

WebCoreOpaqueRoot root(ReadableStream*);

} // namespace WebCore
