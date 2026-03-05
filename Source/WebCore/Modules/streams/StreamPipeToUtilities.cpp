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
#include "StreamPipeToUtilities.h"

#include "ContextDestructionObserverInlines.h"
#include "EventLoop.h"
#include "InternalReadableStream.h"
#include "InternalWritableStream.h"
#include "InternalWritableStreamWriter.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableStream.h"
#include "ReadableStreamDefaultReader.h"
#include "ScriptExecutionContextInlines.h"
#include "StreamPipeOptions.h"
#include "WritableStream.h"

namespace WebCore {

static void readableStreamPipeTo(JSDOMGlobalObject&, Ref<ReadableStream>&&, Ref<WritableStream>&&, Ref<ReadableStreamDefaultReader>&&, Ref<InternalWritableStreamWriter>&&, StreamPipeOptions&&, RefPtr<DeferredPromise>&&);

std::optional<Exception> readableStreamPipeTo(JSDOMGlobalObject& globalObject, ReadableStream& source, WritableStream& destination, StreamPipeOptions&& options, RefPtr<DeferredPromise>&& promise)
{
    auto readerOrException = ReadableStreamDefaultReader::create(globalObject, source);
    if (readerOrException.hasException())
        return readerOrException.releaseException();

    auto writerOrException = acquireWritableStreamDefaultWriter(globalObject, destination);
    if (writerOrException.hasException())
        return writerOrException.releaseException();

    source.markAsDisturbed();

    readableStreamPipeTo(globalObject, source, destination, readerOrException.releaseReturnValue(), writerOrException.releaseReturnValue(), WTF::move(options), WTF::move(promise));
    return { };
}

class PipeToDefaultReadRequest;
class StreamPipeToState : public RefCounted<StreamPipeToState>, public ContextDestructionObserver {
public:
    static Ref<StreamPipeToState> create(JSDOMGlobalObject& globalObject, Ref<ReadableStream>&& source, Ref<WritableStream>&& destination, Ref<ReadableStreamDefaultReader>&& reader, Ref<InternalWritableStreamWriter>&& writer, StreamPipeOptions&& options, RefPtr<DeferredPromise>&& promise)
    {
        Ref state = adoptRef(*new StreamPipeToState(protect(globalObject.scriptExecutionContext()).get(), WTF::move(source), WTF::move(destination), WTF::move(reader), WTF::move(writer), WTF::move(options), WTF::move(promise)));

        state->handleSignal();

        state->errorsMustBePropagatedForward(globalObject);
        state->errorsMustBePropagatedBackward();
        state->closingMustBePropagatedForward();
        state->closingMustBePropagatedBackward();

        state->loop();
        return state;
    }
    ~StreamPipeToState();

    // ContextDestructionObserver.
    void NODELETE ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void doWrite(JSC::JSValue);
    JSDOMGlobalObject* globalObject();

    void readableStreamIsClosing();

    class AllPromise : public RefCounted<AllPromise> {
    public:
        static Ref<AllPromise> create(Ref<DeferredPromise>&& promise) { return adoptRef(*new AllPromise(WTF::move(promise))); }
        void resolve()
        {
            ASSERT(m_waitCount);
            if (m_promise && !--m_waitCount)
                std::exchange(m_promise, { })->resolve();
        }

        void reject(JSC::JSValue result)
        {
            if (RefPtr promise = std::exchange(m_promise, { })) {
                promise->rejectWithCallback([&result](auto&) {
                    return result;
                });
            }
        }

        void waitFor(DOMPromise& promise)
        {
            ++m_waitCount;
            promise.whenSettledWithResult([protectedThis = Ref { *this }](auto*, bool isFulfilled, auto result) {
                if (!isFulfilled) {
                    protectedThis->reject(result);
                    return;
                }
                protectedThis->resolve();
            });
        }

    private:
        explicit AllPromise(Ref<DeferredPromise>&& promise)
            : m_promise(WTF::move(promise))
        {
        }

        RefPtr<DeferredPromise> m_promise;
        size_t m_waitCount { 0 };
    };

private:
    StreamPipeToState(ScriptExecutionContext*, Ref<ReadableStream>&&, Ref<WritableStream>&&, Ref<ReadableStreamDefaultReader>&&, Ref<InternalWritableStreamWriter>&&, StreamPipeOptions&&, RefPtr<DeferredPromise>&&);

    void loop();
    void doRead();

    void handleSignal();

    void errorsMustBePropagatedForward(JSDOMGlobalObject&);
    void errorsMustBePropagatedBackward();
    void closingMustBePropagatedForward();
    void closingMustBePropagatedBackward();

    using Action = Function<RefPtr<DOMPromise>()>;
    using GetError = Function<JSC::JSValue(JSDOMGlobalObject&)>&&;
    void shutdownWithAction(Action&&, GetError&& = { });
    void shutdown(GetError&& = { });
    void finalize(GetError&&);

    RefPtr<DOMPromise> waitForPendingReadAndWrite(Action&&);

    const Ref<ReadableStream> m_source;
    const Ref<WritableStream> m_destination;
    const Ref<ReadableStreamDefaultReader> m_reader;
    const Ref<InternalWritableStreamWriter> m_writer;
    const StreamPipeOptions m_options;
    const RefPtr<DeferredPromise> m_promise;

    bool m_readableIsClosing { false };
    bool m_shuttingDown { false };
    WeakPtr<PipeToDefaultReadRequest> m_pendingReadRequest;
    RefPtr<DOMPromise> m_pendingWritePromise;
};

class PipeToDefaultReadRequest : public ReadableStreamReadRequest, public CanMakeWeakPtr<PipeToDefaultReadRequest> {
public:
    static Ref<PipeToDefaultReadRequest> create(Ref<StreamPipeToState>&& state) { return adoptRef(*new PipeToDefaultReadRequest(WTF::move(state))); }

    void whenSettled(Function<void()>&& callback)
    {
        if (!m_isPending) {
            callback();
            return;
        }

        if (m_whenSettledCallback) {
            auto oldCallback = std::exchange(m_whenSettledCallback, { });
            m_whenSettledCallback = [oldCallback = WTF::move(oldCallback), newCallback = WTF::move(callback)] {
                oldCallback();
                newCallback();
            };
            return;
        }

        m_whenSettledCallback = WTF::move(callback);
    }

private:
    explicit PipeToDefaultReadRequest(Ref<StreamPipeToState>&& state)
        : m_state(WTF::move(state))
    {
    }

    void runChunkSteps(JSC::JSValue value) final
    {
        m_state->doWrite(value);
        settle();
    }

    void runCloseSteps() final
    {
        settleAsClosing();
    }

    void runErrorSteps(JSC::JSValue) final
    {
        settleAsClosing();
    }

    void runErrorSteps(Exception&&) final
    {
        settleAsClosing();
    }

    JSDOMGlobalObject* globalObject() final
    {
        return m_state->globalObject();
    }

    void settle()
    {
        m_isPending = false;
        if (m_whenSettledCallback)
            m_whenSettledCallback();
    }

    void settleAsClosing()
    {
        settle();
        m_state->readableStreamIsClosing();
    }

    const Ref<StreamPipeToState> m_state;
    bool m_isPending { true };
    Function<void()> m_whenSettledCallback;
};

// https://streams.spec.whatwg.org/#readable-stream-pipe-to
void readableStreamPipeTo(JSDOMGlobalObject& globalObject, Ref<ReadableStream>&& source, Ref<WritableStream>&& destination, Ref<ReadableStreamDefaultReader>&& reader, Ref<InternalWritableStreamWriter>&& writer, StreamPipeOptions&& options, RefPtr<DeferredPromise>&& promise)
{
    StreamPipeToState::create(globalObject, WTF::move(source), WTF::move(destination), WTF::move(reader), WTF::move(writer), WTF::move(options), WTF::move(promise));
}

static RefPtr<DOMPromise> cancelReadableStream(JSDOMGlobalObject& globalObject, ReadableStream& stream, JSC::JSValue reason)
{
    RefPtr internalReadableStream = stream.internalReadableStream();
    if (!internalReadableStream)
        return stream.cancel(globalObject, reason);

    auto value = internalReadableStream->cancel(globalObject, reason);
    if (!value)
        return nullptr;

    auto* promise = jsCast<JSC::JSPromise*>(value);
    if (!promise)
        return nullptr;

    return DOMPromise::create(globalObject, *promise);
}

StreamPipeToState::StreamPipeToState(ScriptExecutionContext* context, Ref<ReadableStream>&& source, Ref<WritableStream>&& destination, Ref<ReadableStreamDefaultReader>&& reader, Ref<InternalWritableStreamWriter>&& writer, StreamPipeOptions&& options, RefPtr<DeferredPromise>&& promise)
    : ContextDestructionObserver(context)
    , m_source(WTF::move(source))
    , m_destination(WTF::move(destination))
    , m_reader(WTF::move(reader))
    , m_writer(WTF::move(writer))
    , m_options(WTF::move(options))
    , m_promise(WTF::move(promise))
{
}

StreamPipeToState::~StreamPipeToState() = default;

JSDOMGlobalObject* StreamPipeToState::globalObject()
{
    RefPtr context = scriptExecutionContext();
    return context ? JSC::jsCast<JSDOMGlobalObject*>(context->globalObject()) : nullptr;
}

void StreamPipeToState::handleSignal()
{
    RefPtr signal = m_options.signal;
    if (!signal)
        return;

    auto algorithmSteps = [weakThis = WeakPtr { *this }, signal = m_options.signal]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->shutdownWithAction([protectedThis, signal] -> RefPtr<DOMPromise> {
            RefPtr promiseDestination = [&] -> RefPtr<DOMPromise> {
                bool shouldAbortDestination = !protectedThis->m_options.preventAbort && protectedThis->m_destination->state() == WritableStream::State::Writable;
                if (!shouldAbortDestination)
                    return nullptr;

                Ref internalWritableStream = protectedThis->m_destination->internalWritableStream();
                auto* globalObject = protectedThis->globalObject();
                if (!globalObject)
                    return nullptr;

                auto value = internalWritableStream->abort(*globalObject, signal->reason().getValue());
                auto* promise = jsCast<JSC::JSPromise*>(value);
                if (!promise)
                    return nullptr;

                return DOMPromise::create(*globalObject, *promise);
            }();

            RefPtr promiseSource = [&] -> RefPtr<DOMPromise> {
                bool shouldAbortSource = !protectedThis->m_options.preventCancel && protectedThis->m_source->state() == ReadableStream::State::Readable;
                if (!shouldAbortSource)
                    return nullptr;

                Ref internalWritableStream = protectedThis->m_destination->internalWritableStream();
                auto* globalObject = protectedThis->globalObject();
                if (!globalObject)
                    return nullptr;

                return cancelReadableStream(*globalObject, protectedThis->m_source, signal->reason().getValue());
            }();

            if (!promiseSource && !promiseDestination)
                return nullptr;

            auto* globalObject = protectedThis->globalObject();
            if (!globalObject)
                return nullptr;

            auto [result, deferred] = createPromiseAndWrapper(*globalObject);
            Ref allPromise = AllPromise::create(WTF::move(deferred));
            if (promiseDestination)
                allPromise->waitFor(*promiseDestination);
            if (promiseSource)
                allPromise->waitFor(*promiseSource);

            return RefPtr { WTF::move(result) };
        }, [signal](auto&) { return signal->reason().getValue(); });
    };

    if (m_options.signal->aborted()) {
        algorithmSteps();
        return;
    }

    signal->addAlgorithm([algorithmSteps = WTF::move(algorithmSteps)](auto&&) mutable {
        algorithmSteps();
    });
}

void StreamPipeToState::loop()
{
    if (!m_shuttingDown)
        doRead();
}

void StreamPipeToState::doRead()
{
    ASSERT(!m_shuttingDown);

    m_writer->whenReady([protectedThis = Ref { *this }](bool isRunning) {
        if (!isRunning) {
            // We need to protect |this| until the error callback is called.
            protectedThis->m_writer->onClosedPromiseRejection([protectedThis](auto&, auto&&) { });
            return;
        }

        auto* globalObject = protectedThis->globalObject();
        if (protectedThis->m_shuttingDown || !globalObject)
            return;

        Ref readRequest = PipeToDefaultReadRequest::create(protectedThis.get());
        protectedThis->m_pendingReadRequest = readRequest.get();
        protectedThis->m_reader->read(*globalObject, readRequest.get());
    });
}

void StreamPipeToState::doWrite(JSC::JSValue value)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    m_pendingReadRequest = nullptr;
    m_pendingWritePromise = writableStreamDefaultWriterWrite(m_writer, value);
    if (!m_pendingWritePromise)
        return;

    RefPtr { m_pendingWritePromise }->markAsHandled();

    loop();
}

void StreamPipeToState::errorsMustBePropagatedForward(JSDOMGlobalObject& globalObject)
{
    auto propagateErrorSteps = [weakThis = WeakPtr { *this }](auto&& error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (!protectedThis->m_options.preventAbort) {
            protectedThis->shutdownWithAction([protectedThis, error] -> RefPtr<DOMPromise> {
                auto* globalObject = protectedThis->globalObject();
                if (!globalObject)
                    return nullptr;

                Ref internalWritableStream = protectedThis->m_destination->internalWritableStream();
                auto value = internalWritableStream->abort(*globalObject, error.get());
                auto* promise = jsCast<JSC::JSPromise*>(value);
                if (!promise) {
                    auto [result, deferred] = createPromiseAndWrapper(*globalObject);
                    deferred->resolve();
                    return RefPtr { WTF::move(result) };
                }
                return DOMPromise::create(*globalObject, *promise);
            }, [error](auto&) { return error.get(); });
            return;
        }
        protectedThis->shutdown([error](auto&) { return error.get(); });
    };

    if (m_source->state() == ReadableStream::State::Errored) {
        // FIXME: Check whether ok to take a strong.
        propagateErrorSteps(JSC::Strong<JSC::Unknown> { Ref { m_destination->internalWritableStream().globalObject()->vm() }, m_source->storedError(globalObject) });
        return;
    }

    m_reader->onClosedPromiseRejection([propagateErrorSteps = WTF::move(propagateErrorSteps)](auto& globalObject, auto&& error) mutable {
        // FIXME: Check whether ok to take a strong.
        propagateErrorSteps(JSC::Strong<JSC::Unknown> { Ref { globalObject.vm() }, error });
    });
}

void StreamPipeToState::errorsMustBePropagatedBackward()
{
    auto propagateErrorSteps = [weakThis = WeakPtr { *this }](auto&& error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (!protectedThis->m_options.preventCancel) {
            protectedThis->shutdownWithAction([protectedThis, error] -> RefPtr<DOMPromise> {
                RefPtr internalReadableStream = protectedThis->m_source->internalReadableStream();
                if (!internalReadableStream)
                    return nullptr;

                auto* globalObject = internalReadableStream->globalObject();
                if (!globalObject)
                    return nullptr;

                auto getError2 = [error](auto&) {
                    return error.get();
                };

                auto [result, deferred] = createPromiseAndWrapper(*globalObject);
                auto cancelPromise = cancelReadableStream(*globalObject, protectedThis->m_source, error.get());
                if (!cancelPromise)
                    deferred->rejectWithCallback(WTF::move(getError2), RejectAsHandled::Yes);
                else {
                    cancelPromise->whenSettledWithResult([deferred = WTF::move(deferred), getError2 = WTF::move(getError2)](auto*, bool isFulfilled, auto result) mutable {
                        if (!isFulfilled) {
                            deferred->rejectWithCallback([&result](auto&) {
                                return result;
                            }, RejectAsHandled::Yes);
                            return;
                        }
                        deferred->rejectWithCallback(WTF::move(getError2), RejectAsHandled::Yes);
                    });
                }
                return RefPtr { WTF::move(result) };
            }, [error](auto&) { return error.get(); });
            return;
        }
        protectedThis->shutdown([error](auto&) { return error.get(); });
    };

    if (m_destination->state() == WritableStream::State::Errored) {
        // FIXME: Check whether ok to take a strong.
        auto errorOrException = Ref { m_destination->internalWritableStream() }->storedError();
        if (errorOrException.hasException())
            return;

        propagateErrorSteps(JSC::Strong<JSC::Unknown> { Ref { m_destination->internalWritableStream().globalObject()->vm() }, errorOrException.releaseReturnValue() });
        return;
    }

    m_writer->onClosedPromiseRejection([propagateErrorSteps = WTF::move(propagateErrorSteps)](auto& globalObject, auto&& error) mutable {
        // FIXME: Check whether ok to take a strong.
        propagateErrorSteps(JSC::Strong<JSC::Unknown> { Ref { globalObject.vm() }, error });
    });
}

void StreamPipeToState::readableStreamIsClosing()
{
    // We extend the lifetime of |this| so that StreamPipeToState::closingMustBePropagatedForward lambda is called.
    m_reader->onClosedPromiseResolution([protectedThis = RefPtr { this }] { });
}

void StreamPipeToState::closingMustBePropagatedForward()
{
    auto propagateClosedSteps = [weakThis = WeakPtr { *this }]() {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!protectedThis->m_shuttingDown)
            protectedThis->m_readableIsClosing = true;
        if (!protectedThis->m_options.preventClose) {
            protectedThis->shutdownWithAction([protectedThis] -> RefPtr<DOMPromise> {
                return writableStreamDefaultWriterCloseWithErrorPropagation(protectedThis->m_writer);
            });
            return;
        }
        protectedThis->shutdown();
    };

    if (m_source->state() == ReadableStream::State::Closed) {
        propagateClosedSteps();
        return;
    }

    m_reader->onClosedPromiseResolution(WTF::move(propagateClosedSteps));
}

void StreamPipeToState::closingMustBePropagatedBackward()
{
    if (!Ref { m_destination->internalWritableStream() }->closeQueuedOrInFlight() && m_destination->state() != WritableStream::State::Closed)
        return;

    // FIXME: Assert that no chunks have been read or written.

    auto getError = [](auto& globalObject) {
        return createDOMException(globalObject, Exception { ExceptionCode::TypeError, "closing is propagated backward"_s });
    };

    if (!m_options.preventCancel) {
        shutdownWithAction([protectedThis = Ref { *this }, getError = WTF::move(getError)]() mutable -> RefPtr<DOMPromise> {
            RefPtr internalReadableStream = protectedThis->m_source->internalReadableStream();
            if (!internalReadableStream)
                return nullptr;

            auto* globalObject = protectedThis->globalObject();
            if (!globalObject)
                return nullptr;

            Ref vm = globalObject->vm();
            // FIXME: Check whether ok to take a strong.
            JSC::Strong<JSC::Unknown> error { vm, getError(*globalObject) };
            auto value = internalReadableStream->cancel(*globalObject, error.get());
            if (!value)
                return nullptr;

            auto getError2 = [error = WTF::move(error)](auto&) {
                return error.get();
            };

            auto [result, deferred] = createPromiseAndWrapper(*globalObject);
            auto* promise = jsCast<JSC::JSPromise*>(value);
            if (!promise)
                deferred->rejectWithCallback(WTF::move(getError2), RejectAsHandled::Yes);
            else {
                Ref cancelPromise = DOMPromise::create(*globalObject, *promise);
                cancelPromise->whenSettledWithResult([deferred = WTF::move(deferred), getError2 = WTF::move(getError2)](auto*, bool isFulfilled, auto result) mutable {
                    if (!isFulfilled) {
                        deferred->rejectWithCallback([&result](auto&) {
                            return result;
                        }, RejectAsHandled::Yes);
                        return;
                    }
                    deferred->rejectWithCallback(WTF::move(getError2), RejectAsHandled::Yes);
                });
            }
            return RefPtr { WTF::move(result) };
        });
        return;
    }

    shutdown(WTF::move(getError));
}

RefPtr<DOMPromise> StreamPipeToState::waitForPendingReadAndWrite(Action&& action)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return { };

    RefPtr<DOMPromise> finalizePromise;
    if (m_destination->state() == WritableStream::State::Writable && !Ref { m_destination->internalWritableStream() }->closeQueuedOrInFlight()) {
        if (m_pendingReadRequest || m_pendingWritePromise) {
            auto handlePendingWritePromise = [this, protectedThis = Ref { *this }, action = WTF::move(action)](auto&& deferred) mutable {
                auto waitForAction = [action = WTF::move(action)](auto&& deferred) {
                    RefPtr promise = action();
                    if (!promise) {
                        deferred->resolve();
                        return;
                    }
                    promise->whenSettledWithResult([deferred = WTF::move(deferred)](auto*, bool isFulfilled, auto result) {
                        if (!isFulfilled) {
                            deferred->rejectWithCallback([&result](auto&) {
                                return result;
                            }, RejectAsHandled::Yes);
                            return;
                        }
                        deferred->resolve();
                    });
                };

                if (!m_pendingWritePromise) {
                    waitForAction(WTF::move(deferred));
                    return;
                }

                auto* globalObject = protectedThis->globalObject();
                if (!globalObject) {
                    waitForAction(WTF::move(deferred));
                    return;
                }

                m_pendingWritePromise->whenSettled([waitForAction = WTF::move(waitForAction), deferred = WTF::move(deferred)]() mutable {
                    waitForAction(WTF::move(deferred));
                });
            };

            auto [promise, deferred] = createPromiseAndWrapper(*globalObject);
            // We only wait for the read request if the readable is closing as we know the read request is already fulfilled.
            // The builtin implementation though may resolve the closed promise callback before the read request steps.
            RefPtr readRequest = m_readableIsClosing ? m_pendingReadRequest.get() : nullptr;
            if (readRequest) {
                readRequest->whenSettled([handlePendingWritePromise = WTF::move(handlePendingWritePromise), deferred = WTF::move(deferred)]() mutable {
                    handlePendingWritePromise(WTF::move(deferred));
                });
            } else
                handlePendingWritePromise(WTF::move(deferred));

            finalizePromise = WTF::move(promise);
        }
    }

    if (!finalizePromise)
        finalizePromise = action();

    return finalizePromise;
}

void StreamPipeToState::shutdownWithAction(Action&& action, GetError&& getError)
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;

    RefPtr finalizePromise = waitForPendingReadAndWrite(WTF::move(action));
    if (!finalizePromise) {
        finalize(WTF::move(getError));
        return;
    }

    finalizePromise->whenSettled([protectedThis = Ref { *this }, finalizePromise, getError = WTF::move(getError)]() mutable {
        switch (finalizePromise->status()) {
        case DOMPromise::Status::Fulfilled:
            protectedThis->finalize(WTF::move(getError));
            return;
        case DOMPromise::Status::Rejected:
            protectedThis->finalize([&](auto&) {
                return finalizePromise->result();
            });
            return;
        case DOMPromise::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        }
    });
}

void StreamPipeToState::shutdown(GetError&& getError)
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;

    RefPtr finalizePromise = waitForPendingReadAndWrite([] { return nullptr; });
    if (!finalizePromise) {
        finalize(WTF::move(getError));
        return;
    }

    finalizePromise->whenSettled([protectedThis = Ref { *this }, finalizePromise, getError = WTF::move(getError)]() mutable {
        ASSERT(finalizePromise->status() != DOMPromise::Status::Pending);
        protectedThis->finalize(WTF::move(getError));
    });
}

void StreamPipeToState::finalize(GetError&& getError)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    writableStreamDefaultWriterRelease(m_writer);
    m_reader->releaseLock(*globalObject);

    m_pendingReadRequest = nullptr;
    m_pendingWritePromise = nullptr;

    if (!m_promise)
        return;

    if (getError) {
        m_promise->rejectWithCallback(WTF::move(getError), RejectAsHandled::No);
        return;
    }

    m_promise->resolve();
}

}
