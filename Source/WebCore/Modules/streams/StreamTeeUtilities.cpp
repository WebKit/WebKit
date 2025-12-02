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
#include "StreamTeeUtilities.h"

#include "ContextDestructionObserverInlines.h"
#include "EventLoop.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSReadableStreamReadResult.h"
#include "JSValueInWrappedObject.h"
#include "ReadableByteStreamController.h"
#include "ReadableStream.h"
#include "ReadableStreamBYOBReader.h"
#include "ReadableStreamBYOBRequest.h"
#include "ReadableStreamDefaultReader.h"
#include "ScriptExecutionContextInlines.h"

namespace WebCore {

class StreamTeeState final : public ReadableStream::DependencyToVisit, public RefCounted<StreamTeeState>, public ContextDestructionObserver {
public:
    template<typename Reader>
    static Ref<StreamTeeState> create(JSDOMGlobalObject& globalObject, Ref<ReadableStream>&& stream, Ref<Reader>&& reader)
    {
        auto [cancelPromise, cancelDeferred] = createPromiseAndWrapper(globalObject);
        return adoptRef(*new StreamTeeState(globalObject.protectedScriptExecutionContext().get(), WTFMove(stream), WTFMove(reader), WTFMove(cancelDeferred), WTFMove(cancelPromise)));
    }

    ~StreamTeeState();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    bool isReader(const ReadableStreamDefaultReader* thisReader) const { return m_defaultReader && m_defaultReader.get() == thisReader; }
    bool isReader(const ReadableStreamBYOBReader* thisReader) const { return m_byobReader && m_byobReader.get() == thisReader; }

    bool reading() const { return m_reading; }
    void setReading(bool value) { m_reading = value; }

    bool readAgainForBranch1() const { return m_readAgainForBranch1; }
    void setReadAgainForBranch1(bool value) { m_readAgainForBranch1 = value; }

    bool readAgainForBranch2() const { return m_readAgainForBranch2; }
    void setReadAgainForBranch2(bool value) { m_readAgainForBranch2 = value; }

    bool canceled1() const { return m_canceled1; }
    bool canceled2() const { return m_canceled2; }
    void setCanceled1() { m_canceled1 = true; }
    void setCanceled2() { m_canceled2 = true; }
    JSC::JSValue reason1() { return m_branch1Reason.getValue(); }
    JSC::JSValue reason2() { return m_branch2Reason.getValue(); }
    void setReason1(JSDOMGlobalObject& globalObject, const JSC::JSCell* owner, JSC::JSValue value)
    {
        Ref vm = globalObject.vm();
        m_branch1Reason.set(vm, owner, value);
    }
    void setReason2(JSDOMGlobalObject& globalObject, const JSC::JSCell* owner, JSC::JSValue value)
    {
        Ref vm = globalObject.vm();
        m_branch2Reason.set(vm, owner, value);
    }
    void visit(JSC::AbstractSlotVisitor& visitor) final
    {
        m_branch1Reason.visit(visitor);
        m_branch2Reason.visit(visitor);
    }
    void clearReasons()
    {
        m_branch1Reason.clear();
        m_branch2Reason.clear();
    }

    ReadableStream& stream() const { return m_stream; }
    ReadableStream* branch1() const { return m_branch1.get(); }
    ReadableStream* branch2() const { return m_branch2.get(); }
    void setBranch1(ReadableStream& stream) { m_branch1 = &stream; }
    void setBranch2(ReadableStream& stream) { m_branch2 = &stream; }

    ReadableStreamBYOBReader* byobReader() const { return m_byobReader.get(); }
    RefPtr<ReadableStreamBYOBReader> takeBYOBReader() { return std::exchange(m_byobReader, { }); }
    void setReader(Ref<ReadableStreamBYOBReader>&& reader)
    {
        ASSERT(!m_defaultReader);
        ASSERT(!m_byobReader);
        m_byobReader = WTFMove(reader);
    }

    ReadableStreamDefaultReader* defaultReader() const { return m_defaultReader.get(); }
    RefPtr<ReadableStreamDefaultReader> takeDefaultReader() { return std::exchange(m_defaultReader, { }); }
    void setReader(Ref<ReadableStreamDefaultReader>&& reader)
    {
        ASSERT(!m_defaultReader);
        ASSERT(!m_byobReader);
        m_defaultReader = WTFMove(reader);
    }

    DOMPromise& cancelPromise() { return m_cancelPromise; }

    void resolveCancelPromise()
    {
        Ref { m_cancelDeferredPromise }->resolve();
    }

    void rejectCancelPromise(JSC::JSValue value)
    {
        Ref { m_cancelDeferredPromise }->rejectWithCallback([&](auto&) {
            return value;
        });
    }

    template<typename Reader>
    void forwardReadError(Reader& thisReader)
    {
        thisReader.onClosedPromiseRejection([weakThis = WeakPtr { *this }, thisReader = WeakPtr { thisReader }](auto& globalObject, auto&& reason) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !protectedThis->isReader(thisReader.get()))
                return;

            if (RefPtr branch1 = protectedThis->branch1())
                branch1->controller()->error(globalObject, reason);
            if (RefPtr branch2 = protectedThis->branch2())
                branch2->controller()->error(globalObject, reason);
            if (!protectedThis->canceled1() || !protectedThis->canceled2())
                protectedThis->resolveCancelPromise();
        });
    }

    JSDOMGlobalObject* globalObject()
    {
        RefPtr context = scriptExecutionContext();
        return context ? JSC::jsCast<JSDOMGlobalObject*>(context->globalObject()) : nullptr;
    }

    void queueMicrotaskWithValue(JSC::JSValue value, Function<void(JSC::JSValue)>&& task)
    {
        RefPtr context = scriptExecutionContext();
        if (!context)
            return;

        auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(context->globalObject());
        context->checkedEventLoop()->queueMicrotask([task = WTFMove(task), value = JSC::Strong<JSC::Unknown> { globalObject.vm(), value }] {
            task(value.get());
        });
    }

    void contextDestroyed() final
    {
        m_defaultReader = nullptr;
        m_byobReader = nullptr;
        m_branch1 = nullptr;
        m_branch2 = nullptr;
        clearReasons();
    }

private:
    StreamTeeState(ScriptExecutionContext* context, Ref<ReadableStream>&& stream, Ref<ReadableStreamDefaultReader>&& reader, Ref<DeferredPromise>&& cancelDeferred, Ref<DOMPromise>&& cancelPromise)
        : ContextDestructionObserver(context)
        , m_stream(WTFMove(stream))
        , m_defaultReader(WTFMove(reader))
        , m_cancelDeferredPromise(WTFMove(cancelDeferred))
        , m_cancelPromise(WTFMove(cancelPromise))
    {
    }

    StreamTeeState(ScriptExecutionContext* context, Ref<ReadableStream>&& stream, Ref<ReadableStreamBYOBReader>&& reader, Ref<DeferredPromise>&& cancelDeferred, Ref<DOMPromise>&& cancelPromise)
        : ContextDestructionObserver(context)
        , m_stream(WTFMove(stream))
        , m_byobReader(WTFMove(reader))
        , m_cancelDeferredPromise(WTFMove(cancelDeferred))
        , m_cancelPromise(WTFMove(cancelPromise))
    {
    }

    const Ref<ReadableStream> m_stream;
    RefPtr<ReadableStreamDefaultReader> m_defaultReader;
    RefPtr<ReadableStreamBYOBReader> m_byobReader;
    bool m_reading = false;
    bool m_readAgainForBranch1 = false;
    bool m_readAgainForBranch2 = false;
    bool m_canceled1 = false;
    bool m_canceled2 = false;
    Ref<DeferredPromise> m_cancelDeferredPromise;
    Ref<DOMPromise> m_cancelPromise;
    RefPtr<ReadableStream> m_branch1;
    RefPtr<ReadableStream> m_branch2;
    JSValueInWrappedObject m_branch1Reason;
    JSValueInWrappedObject m_branch2Reason;
};

// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
ExceptionOr<Vector<Ref<ReadableStream>>> byteStreamTee(JSDOMGlobalObject& globalObject, ReadableStream& stream)
{
    ASSERT(stream.controller());

    auto readerOrException = ReadableStreamDefaultReader::create(globalObject, stream);
    if (readerOrException.hasException())
        return readerOrException.releaseException();

    Ref reader = readerOrException.releaseReturnValue();
    Ref state = StreamTeeState::create(globalObject, stream, reader.copyRef());

    ReadableByteStreamController::PullAlgorithm pull1Algorithm = [state = Ref { state }](auto& globalObject, auto&&) {
        return pull1Steps(globalObject, state, Ref { *state->branch1() });
    };

    ReadableByteStreamController::PullAlgorithm pull2Algorithm = [state = Ref { state }](auto& globalObject, auto&&) {
        return pull2Steps(globalObject, state, Ref { *state->branch2() });
    };

    ReadableByteStreamController::CancelAlgorithm cancel1Algorithm = [state = Ref { state }](auto& globalObject, auto&&, auto&& reason) {
        state->setCanceled1();
        state->setReason1(globalObject, &globalObject, reason.value_or(JSC::jsUndefined()));

        if (state->canceled2()) {
            // Create the array of reason1 and reason2.
            JSC::MarkedArgumentBuffer list;
            list.ensureCapacity(2);
            list.append(state->reason1());
            list.append(state->reason2());
            JSC::JSValue reason = JSC::constructArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list);

            Ref promise = state->stream().cancel(globalObject, reason);
            promise->whenSettled([state, promise] {
                if (promise->status() == DOMPromise::Status::Rejected) {
                    state->rejectCancelPromise(promise->result());
                    return;
                }
                state->resolveCancelPromise();
            });
            state->clearReasons();
        }
        return Ref { state->cancelPromise() };
    };

    ReadableByteStreamController::CancelAlgorithm cancel2Algorithm = [state = Ref { state }](auto& globalObject, auto&&, auto&& reason) {
        state->setCanceled2();
        state->setReason2(globalObject, &globalObject, reason.value_or(JSC::jsUndefined()));

        if (state->canceled1()) {
            // Create the array of reason1 and reason2.
            JSC::MarkedArgumentBuffer list;
            list.ensureCapacity(2);
            list.append(state->reason1());
            list.append(state->reason2());
            JSC::JSValue reason = JSC::constructArray(&globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), list);

            Ref promise = state->stream().cancel(globalObject, reason);
            promise->whenSettled([state, promise] {
                if (promise->status() == DOMPromise::Status::Rejected) {
                    state->rejectCancelPromise(promise->result());
                    return;
                }
                state->resolveCancelPromise();
            });
            state->clearReasons();
        }
        return Ref { state->cancelPromise() };
    };

    Vector<Ref<ReadableStream>> branches;
    Ref branch0 = ReadableStream::createReadableByteStream(globalObject, WTFMove(pull1Algorithm), WTFMove(cancel1Algorithm), {
        .dependencyToVisit = state.ptr()
    });
    Ref branch1 = ReadableStream::createReadableByteStream(globalObject, WTFMove(pull2Algorithm), WTFMove(cancel2Algorithm), {
        .dependencyToVisit = state.ptr()
    });

    state->setBranch1(branch0);
    state->setBranch2(branch1);

    branches.append(WTFMove(branch0));
    branches.append(WTFMove(branch1));

    state->forwardReadError(reader.get());

    return branches;
}

StreamTeeState::~StreamTeeState() = default;

static ExceptionOr<Ref<JSC::ArrayBufferView>> cloneAsUInt8Array(JSC::ArrayBufferView& o)
{
    RefPtr buffer = JSC::ArrayBuffer::tryCreate(o.span());
    if (!buffer)
        return Exception { ExceptionCode::OutOfMemoryError };

    Ref<JSC::ArrayBufferView> clone = JSC::Uint8Array::create(WTFMove(buffer), 0, o.byteLength());

    return clone;
}

static void pullWithBYOBReader(JSDOMGlobalObject&, StreamTeeState&, ReadableStreamBYOBRequest&, bool);
static void pullWithDefaultReader(JSDOMGlobalObject&, StreamTeeState&);

static Ref<DOMPromise> pull1Steps(JSDOMGlobalObject& globalObject, StreamTeeState& state, ReadableStream& branch1)
{
    if (state.reading()) {
        state.setReadAgainForBranch1(true);
        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        deferred->resolve();
        return promise;
    }

    state.setReading(true);

    RefPtr byobRequest = branch1.protectedController()->getByobRequest();
    if (!byobRequest)
        pullWithDefaultReader(globalObject, state);
    else
        pullWithBYOBReader(globalObject, state, *byobRequest, false);

    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    deferred->resolve();
    return promise;
}

static Ref<DOMPromise> pull2Steps(JSDOMGlobalObject& globalObject, StreamTeeState& state, ReadableStream& branch2)
{
    if (state.reading()) {
        state.setReadAgainForBranch2(true);
        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        deferred->resolve();
        return promise;
    }

    state.setReading(true);

    RefPtr byobRequest = branch2.protectedController()->getByobRequest();
    if (!byobRequest)
        pullWithDefaultReader(globalObject, state);
    else
        pullWithBYOBReader(globalObject, state, *byobRequest, true);

    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    deferred->resolve();
    return promise;
}

class TeeDefaultReadRequest : public ReadableStreamReadRequest {
public:
    static Ref<TeeDefaultReadRequest> create(Ref<StreamTeeState>&& state) { return adoptRef(*new TeeDefaultReadRequest(WTFMove(state))); }

private:
    explicit TeeDefaultReadRequest(Ref<StreamTeeState>&& state)
        : m_state(WTFMove(state))
    {
    }

    void runChunkSteps(JSC::JSValue value) final
    {
        m_state->queueMicrotaskWithValue(value, [protectedThis = Ref { *this }](auto value) {
            protectedThis->runChunkStepsInMicrotask(value);
        });
    }

    void runChunkStepsInMicrotask(JSC::JSValue value)
    {
        auto* globalObject = this->globalObject();
        if (!globalObject)
            return;

        RefPtr branch1 = m_state->branch1();
        RefPtr branch2 = m_state->branch2();

        m_state->setReadAgainForBranch1(false);
        m_state->setReadAgainForBranch2(false);

        auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
        auto chunkResult = convert<IDLArrayBufferView>(*globalObject, value);
        if (chunkResult.hasException(scope)) [[unlikely]] {
            scope.clearException();
            return;
        }

        Ref chunk1 = chunkResult.releaseReturnValue();
        Ref chunk2 = chunk1;

        if (!m_state->canceled1() && !m_state->canceled2()) {
            auto resultOrException = cloneAsUInt8Array(chunk1);
            if (resultOrException.hasException()) {
                if (branch1)
                    branch1->controller()->error(*globalObject, resultOrException.exception());
                if (branch2)
                    branch2->controller()->error(*globalObject, resultOrException.exception());

                m_state->stream().cancel(resultOrException.releaseException());
                return;
            }
            chunk2 = resultOrException.releaseReturnValue();
        }
        if (!m_state->canceled1() && branch1)
            branch1->protectedController()->enqueue(*globalObject, chunk1);
        if (!m_state->canceled2() && branch2)
            branch2->protectedController()->enqueue(*globalObject, chunk2);

        m_state->setReading(false);
        if (m_state->readAgainForBranch1() && branch1)
            pull1Steps(*globalObject, m_state, *branch1);
        else if (m_state->readAgainForBranch2() && branch2)
            pull2Steps(*globalObject, m_state, *branch2);
    }

    void runCloseSteps() final
    {
        auto* globalObject = this->globalObject();
        if (!globalObject)
            return;

        RefPtr branch1 = m_state->branch1();
        RefPtr branch2 = m_state->branch2();

        m_state->setReading(false);
        if (!m_state->canceled1() && branch1)
            branch1->controller()->close(*globalObject);
        if (!m_state->canceled2() && branch2)
            branch2->controller()->close(*globalObject);

        if (branch1 && branch1->protectedController()->hasPendingPullIntos())
            branch1->protectedController()->respond(*globalObject, 0);
        if (branch2 && branch2->protectedController()->hasPendingPullIntos())
            branch2->protectedController()->respond(*globalObject, 0);

        if (!m_state->canceled1() || !m_state->canceled2())
            m_state->resolveCancelPromise();
    }

    void runErrorSteps(JSC::JSValue) final
    {
        runErrorSteps();
    }

    void runErrorSteps(Exception&&) final
    {
        runErrorSteps();
    }

    void runErrorSteps()
    {
        m_state->setReading(false);
    }

    JSDOMGlobalObject* globalObject() final
    {
        return m_state->globalObject();
    }

    const Ref<StreamTeeState> m_state;
};

class TeeBYOBReadRequest : public ReadableStreamReadIntoRequest {
public:
    static Ref<TeeBYOBReadRequest> create(Ref<StreamTeeState>&& state, bool forBranch2) { return adoptRef(*new TeeBYOBReadRequest(WTFMove(state), forBranch2)); }

private:
    explicit TeeBYOBReadRequest(Ref<StreamTeeState>&& state, bool forBranch2)
        : m_state(WTFMove(state))
        , m_forBranch2(forBranch2)
    {
    }

    void runChunkSteps(JSC::JSValue value) final
    {
        m_state->queueMicrotaskWithValue(value, [protectedThis = Ref { *this }](auto value) {
            protectedThis->runChunkStepsInMicrotask(value);
        });
    }

    void runChunkStepsInMicrotask(JSC::JSValue value)
    {
        auto* globalObject = this->globalObject();
        if (!globalObject)
            return;

        RefPtr branch1 = m_state->branch1();
        RefPtr branch2 = m_state->branch2();

        auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
        auto chunkResult = convert<IDLArrayBufferView>(*globalObject, value);
        if (chunkResult.hasException(scope)) [[unlikely]] {
            scope.clearException();
            return;
        }

        Ref chunk = chunkResult.releaseReturnValue();

        m_state->setReadAgainForBranch1(false);
        m_state->setReadAgainForBranch2(false);

        bool byobCanceled = m_forBranch2 ? m_state->canceled2() : m_state->canceled1();
        bool otherCanceled = m_forBranch2 ? m_state->canceled1() : m_state->canceled2();

        RefPtr byobBranch = m_forBranch2 ? branch2 : branch1;
        RefPtr otherBranch = m_forBranch2 ? branch1 : branch2;
        if (!otherCanceled) {
            auto resultOrException = cloneAsUInt8Array(chunk);
            if (resultOrException.hasException()) {
                if (byobBranch)
                    byobBranch->controller()->error(*globalObject, resultOrException.exception());
                if (otherBranch)
                    otherBranch->controller()->error(*globalObject, resultOrException.exception());

                m_state->stream().cancel(resultOrException.releaseException());
                return;
            }
            Ref clonedChunk = resultOrException.releaseReturnValue();
            if (!byobCanceled)
                byobBranch->protectedController()->respondWithNewView(*globalObject, chunk);
            otherBranch->protectedController()->enqueue(*globalObject, clonedChunk);
        } else if (!byobCanceled)
            byobBranch->protectedController()->respondWithNewView(*globalObject, chunk);

        m_state->setReading(false);
        if (m_state->readAgainForBranch1() && branch1)
            pull1Steps(*globalObject, m_state, *branch1);
        else if (m_state->readAgainForBranch2() && branch2)
            pull2Steps(*globalObject, m_state, *branch2);
    }

    void runCloseSteps(JSC::JSValue value) final
    {
        auto* globalObject = this->globalObject();
        if (!globalObject)
            return;

        RefPtr branch1 = m_state->branch1();
        RefPtr branch2 = m_state->branch2();

        m_state->setReading(false);
        bool byobCanceled = m_forBranch2 ? m_state->canceled2() : m_state->canceled1();
        bool otherCanceled = m_forBranch2 ? m_state->canceled1() : m_state->canceled2();
        if (!byobCanceled && branch1)
            branch1->controller()->close(*globalObject);

        if (!otherCanceled && branch2)
            branch2->controller()->close(*globalObject);

        if (!value.isUndefined()) {
            auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
            auto chunkResult = convert<IDLArrayBufferView>(*globalObject, value);
            if (chunkResult.hasException(scope)) [[unlikely]] {
                scope.clearException();
                return;
            }

            Ref chunk = chunkResult.releaseReturnValue();
            ASSERT(!chunk->byteLength());

            if (!byobCanceled && branch1)
                branch1->protectedController()->respondWithNewView(*globalObject, chunk);
            if (!otherCanceled && branch2 && branch2->controller()->hasPendingPullIntos())
                branch2->protectedController()->respond(*globalObject, 0);
        }

        if (!byobCanceled || !otherCanceled)
            m_state->resolveCancelPromise();
    }

    void runErrorSteps(JSC::JSValue) final
    {
        runErrorSteps();
    }

    void runErrorSteps(Exception&&) final
    {
        runErrorSteps();
    }

    void runErrorSteps()
    {
        m_state->setReading(false);
    }

    JSDOMGlobalObject* globalObject() final
    {
        return m_state->globalObject();
    }

    const Ref<StreamTeeState> m_state;
    const bool m_forBranch2 { false };
};

static void pullWithDefaultReader(JSDOMGlobalObject& globalObject, StreamTeeState& state)
{
    if (RefPtr byobReader = state.takeBYOBReader()) {
        ASSERT(!byobReader->readIntoRequestsSize());
        byobReader->releaseLock(globalObject);

        auto readerOrException = ReadableStreamDefaultReader::create(globalObject, Ref { state.stream() }.get());
        if (readerOrException.hasException()) {
            ASSERT_NOT_REACHED();
            return;
        }
        Ref reader = readerOrException.releaseReturnValue();
        state.setReader(reader.get());
        state.forwardReadError(reader.get());
    }

    RefPtr reader = state.defaultReader();
    reader->read(globalObject, TeeDefaultReadRequest::create(state));
}

static void pullWithBYOBReader(JSDOMGlobalObject& globalObject, StreamTeeState& state, ReadableStreamBYOBRequest& request, bool forBranch2)
{
    if (RefPtr defaultReader = state.takeDefaultReader()) {
        ASSERT(!defaultReader->getNumReadRequests());
        defaultReader->releaseLock(globalObject);

        auto readerOrException = ReadableStreamBYOBReader::create(globalObject, Ref { state.stream() }.get());
        if (readerOrException.hasException()) {
            ASSERT_NOT_REACHED();
            return;
        }
        Ref reader = readerOrException.releaseReturnValue();
        state.setReader(reader.get());
        state.forwardReadError(reader.get());
    }

    RefPtr reader = state.byobReader();
    RefPtr byobBranch = forBranch2 ? state.branch2() : state.branch1();
    RefPtr otherBranch = forBranch2 ? state.branch1() : state.branch2();

    reader->read(globalObject, Ref { *request.view() }, 1, TeeBYOBReadRequest::create(state, forBranch2));
}


} // namespace WebCore
