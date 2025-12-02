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

class PipeToDefaultReadRequest;
class StreamPipeToState : public RefCounted<StreamPipeToState>, public ContextDestructionObserver {
public:
    static Ref<StreamPipeToState> create(JSDOMGlobalObject& globalObject, Ref<ReadableStream>&& source, Ref<WritableStream>&& destination, Ref<ReadableStreamDefaultReader>&& reader, Ref<InternalWritableStreamWriter>&& writer, StreamPipeOptions&& options, RefPtr<DeferredPromise>&& promise)
    {
        Ref state = adoptRef(*new StreamPipeToState(globalObject.protectedScriptExecutionContext().get(), WTFMove(source), WTFMove(destination), WTFMove(reader), WTFMove(writer), WTFMove(options), WTFMove(promise)));

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
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void doWrite(JSC::JSValue);
    JSDOMGlobalObject* globalObject();

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

    bool m_shuttingDown { false };
    RefPtr<PipeToDefaultReadRequest> m_pendingReadRequest;
    RefPtr<DOMPromise> m_pendingWritePromise;
};

class PipeToDefaultReadRequest : public ReadableStreamReadRequest {
public:
    static Ref<PipeToDefaultReadRequest> create(Ref<StreamPipeToState>&& state) { return adoptRef(*new PipeToDefaultReadRequest(WTFMove(state))); }

    void whenSettled(Function<void()>&& callback)
    {
        if (!m_isPending) {
            callback();
            return;
        }

        if (m_whenSettledCallback) {
            auto oldCallback = std::exchange(m_whenSettledCallback, { });
            m_whenSettledCallback = [oldCallback = WTFMove(oldCallback), newCallback = WTFMove(callback)] {
                oldCallback();
                newCallback();
            };
            return;
        }

        m_whenSettledCallback = WTFMove(callback);
    }

private:
    explicit PipeToDefaultReadRequest(Ref<StreamPipeToState>&& state)
        : m_state(WTFMove(state))
    {
    }

    void runChunkSteps(JSC::JSValue value) final
    {
        m_state->doWrite(value);
        settle();
    }

    void runCloseSteps() final
    {
        settle();
    }

    void runErrorSteps(JSC::JSValue) final
    {
        settle();
    }

    void runErrorSteps(Exception&&) final
    {
        settle();
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

    const Ref<StreamPipeToState> m_state;
    bool m_isPending { true };
    Function<void()> m_whenSettledCallback;
};

// https://streams.spec.whatwg.org/#readable-stream-pipe-to
void readableStreamPipeTo(JSDOMGlobalObject& globalObject, Ref<ReadableStream>&& source, Ref<WritableStream>&& destination, Ref<ReadableStreamDefaultReader>&& reader, Ref<InternalWritableStreamWriter>&& writer, StreamPipeOptions&& options, RefPtr<DeferredPromise>&& promise)
{
    StreamPipeToState::create(globalObject, WTFMove(source), WTFMove(destination), WTFMove(reader), WTFMove(writer), WTFMove(options), WTFMove(promise));
}

static RefPtr<DOMPromise> cancelReadableStream(JSDOMGlobalObject& globalObject, ReadableStream& stream, JSC::JSValue reason)
{
    RefPtr internalReadableStream = stream.internalReadableStream();
    if (!internalReadableStream)
        return stream.cancel(globalObject, reason);

    auto value = internalReadableStream->cancel(globalObject, reason);
    auto* promise = jsCast<JSC::JSPromise*>(value);
    if (!promise)
        return nullptr;

    return DOMPromise::create(globalObject, *promise);
}

StreamPipeToState::StreamPipeToState(ScriptExecutionContext* context, Ref<ReadableStream>&& source, Ref<WritableStream>&& destination, Ref<ReadableStreamDefaultReader>&& reader, Ref<InternalWritableStreamWriter>&& writer, StreamPipeOptions&& options, RefPtr<DeferredPromise>&& promise)
    : ContextDestructionObserver(context)
    , m_source(WTFMove(source))
    , m_destination(WTFMove(destination))
    , m_reader(WTFMove(reader))
    , m_writer(WTFMove(writer))
    , m_options(WTFMove(options))
    , m_promise(WTFMove(promise))
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
            if (promiseDestination) {
                promiseDestination->whenSettled([promiseDestination, promiseSource, deferred] {
                    if (promiseDestination->status() == DOMPromise::Status::Rejected) {
                        deferred->rejectWithCallback([&](auto&) {
                            return promiseDestination->result();
                        }, RejectAsHandled::Yes);
                        return;
                    }
                    if (promiseSource && promiseSource->status() != DOMPromise::Status::Fulfilled)
                        return;
                    deferred->resolve();
                });
            }
            if (promiseSource) {
                promiseSource->whenSettled([promiseDestination, promiseSource, deferred] {
                    if (promiseSource->status() == DOMPromise::Status::Rejected) {
                        deferred->rejectWithCallback([&](auto&) {
                            return promiseSource->result();
                        }, RejectAsHandled::Yes);
                        return;
                    }
                    if (promiseDestination && promiseDestination->status() != DOMPromise::Status::Fulfilled)
                        return;
                    deferred->resolve();
                });
            }

            return RefPtr { WTFMove(result) };
        }, [signal](auto&) { return signal->reason().getValue(); });
    };

    if (m_options.signal->aborted()) {
        algorithmSteps();
        return;
    }

    signal->addAlgorithm([algorithmSteps = WTFMove(algorithmSteps)](auto&&) mutable {
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

    m_writer->whenReady([protectedThis = Ref { *this }] {
        auto* globalObject = protectedThis->globalObject();
        if (protectedThis->m_shuttingDown || !globalObject)
            return;

        protectedThis->m_pendingReadRequest = PipeToDefaultReadRequest::create(protectedThis.get());
        protectedThis->m_reader->read(*globalObject, *protectedThis->m_pendingReadRequest);
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
                    return RefPtr { WTFMove(result) };
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

    m_reader->onClosedPromiseRejection([propagateErrorSteps = WTFMove(propagateErrorSteps)](auto& globalObject, auto&& error) mutable {
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
                    deferred->rejectWithCallback(WTFMove(getError2), RejectAsHandled::Yes);
                else {
                    cancelPromise->whenSettled([deferred = WTFMove(deferred), cancelPromise, getError2 = WTFMove(getError2)]() mutable {
                        if (cancelPromise->status() == DOMPromise::Status::Rejected) {
                            deferred->rejectWithCallback([&](auto&) {
                                return cancelPromise->result();
                            }, RejectAsHandled::Yes);
                            return;
                        }
                        deferred->rejectWithCallback(WTFMove(getError2), RejectAsHandled::Yes);
                    });
                }
                return RefPtr { WTFMove(result) };
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

    m_writer->onClosedPromiseRejection([propagateErrorSteps = WTFMove(propagateErrorSteps)](auto& globalObject, auto&& error) mutable {
        // FIXME: Check whether ok to take a strong.
        propagateErrorSteps(JSC::Strong<JSC::Unknown> { Ref { globalObject.vm() }, error });
    });
}

void StreamPipeToState::closingMustBePropagatedForward()
{
    auto propagateClosedSteps = [weakThis = WeakPtr { *this }]() {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
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

    m_reader->onClosedPromiseResolution(WTFMove(propagateClosedSteps));
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
        shutdownWithAction([protectedThis = Ref { *this }, getError = WTFMove(getError)]() mutable -> RefPtr<DOMPromise> {
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

            auto getError2 = [error = WTFMove(error)](auto&) {
                return error.get();
            };

            auto [result, deferred] = createPromiseAndWrapper(*globalObject);
            auto* promise = jsCast<JSC::JSPromise*>(value);
            if (!promise)
                deferred->rejectWithCallback(WTFMove(getError2), RejectAsHandled::Yes);
            else {
                Ref cancelPromise = DOMPromise::create(*globalObject, *promise);
                cancelPromise->whenSettled([deferred = WTFMove(deferred), cancelPromise, getError2 = WTFMove(getError2)]() mutable {
                    if (cancelPromise->status() == DOMPromise::Status::Rejected) {
                        deferred->rejectWithCallback([&](auto&) {
                            return cancelPromise->result();
                        }, RejectAsHandled::Yes);
                        return;
                    }
                    deferred->rejectWithCallback(WTFMove(getError2), RejectAsHandled::Yes);
                });
            }
            return RefPtr { WTFMove(result) };
        });
        return;
    }

    shutdown(WTFMove(getError));
}

RefPtr<DOMPromise> StreamPipeToState::waitForPendingReadAndWrite(Action&& action)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return { };

    RefPtr<DOMPromise> finalizePromise;
    if (m_destination->state() == WritableStream::State::Writable && !Ref { m_destination->internalWritableStream() }->closeQueuedOrInFlight()) {
        if (m_pendingReadRequest || m_pendingWritePromise) {
            auto handlePendingWritePromise = [this, protectedThis = Ref { *this }, action = WTFMove(action)](auto&& deferred) mutable {
                auto waitForAction = [action = WTFMove(action)](auto&& deferred) {
                    RefPtr promise = action();
                    if (!promise) {
                        deferred->resolve();
                        return;
                    }
                    promise->whenSettled([deferred = WTFMove(deferred), promise] {
                        switch (promise->status()) {
                        case DOMPromise::Status::Rejected:
                            deferred->rejectWithCallback([&](auto&) { return promise->result(); }, RejectAsHandled::Yes);
                            return;
                        case DOMPromise::Status::Fulfilled:
                            deferred->resolve();
                            return;
                        case DOMPromise::Status::Pending:
                            ASSERT_NOT_REACHED();
                            return;
                        }
                    });
                };

                if (!m_pendingWritePromise) {
                    waitForAction(WTFMove(deferred));
                    return;
                }

                auto* globalObject = protectedThis->globalObject();
                if (!globalObject) {
                    waitForAction(WTFMove(deferred));
                    return;
                }

                m_pendingWritePromise->whenSettled([waitForAction = WTFMove(waitForAction), pendingWritePromise = m_pendingWritePromise, deferred = WTFMove(deferred)]() mutable {
                    ASSERT(pendingWritePromise->status() != DOMPromise::Status::Pending);
                    waitForAction(WTFMove(deferred));
                });
            };

            auto [promise, deferred] = createPromiseAndWrapper(*globalObject);
            if (RefPtr readRequest = m_pendingReadRequest) {
                readRequest->whenSettled([handlePendingWritePromise = WTFMove(handlePendingWritePromise), deferred = WTFMove(deferred)]() mutable {
                    handlePendingWritePromise(WTFMove(deferred));
                });
            } else
                handlePendingWritePromise(WTFMove(deferred));

            finalizePromise = WTFMove(promise);
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

    RefPtr finalizePromise = waitForPendingReadAndWrite(WTFMove(action));
    if (!finalizePromise) {
        finalize(WTFMove(getError));
        return;
    }

    finalizePromise->whenSettled([protectedThis = Ref { *this }, finalizePromise, getError = WTFMove(getError)]() mutable {
        switch (finalizePromise->status()) {
        case DOMPromise::Status::Fulfilled:
            protectedThis->finalize(WTFMove(getError));
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
        finalize(WTFMove(getError));
        return;
    }

    finalizePromise->whenSettled([protectedThis = Ref { *this }, finalizePromise, getError = WTFMove(getError)]() mutable {
        ASSERT(finalizePromise->status() != DOMPromise::Status::Pending);
        protectedThis->finalize(WTFMove(getError));
    });
}

void StreamPipeToState::finalize(GetError&& getError)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    writableStreamDefaultWriterRelease(m_writer);
    m_reader->releaseLock(*globalObject);

    if (!m_promise)
        return;

    if (getError) {
        m_promise->rejectWithCallback(WTFMove(getError), RejectAsHandled::No);
        return;
    }

    m_promise->resolve();
}

}
