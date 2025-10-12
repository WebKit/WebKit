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
#include "InternalReadableStream.h"
#include "JSValueInWrappedObject.h"
#include "ReadableByteStreamController.h"
#include <JavaScriptCore/Strong.h>
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
class ReadableStreamBYOBReader;
class ReadableStreamDefaultReader;
class ReadableStreamReadRequest;
class ReadableStreamSource;
class WritableStream;

struct StreamPipeOptions;
struct UnderlyingSource;

using ReadableStreamReader = Variant<RefPtr<ReadableStreamDefaultReader>, RefPtr<ReadableStreamBYOBReader>>;

class ReadableStream : public RefCountedAndCanMakeWeakPtr<ReadableStream>, public ContextDestructionObserver {
public:
    enum class ReaderMode { Byob };
    struct GetReaderOptions {
        std::optional<ReaderMode> mode;
    };
    struct WritablePair {
        RefPtr<ReadableStream> readable;
        RefPtr<WritableStream> writable;
    };

    static ExceptionOr<Ref<ReadableStream>> create(JSDOMGlobalObject&, std::optional<JSC::Strong<JSC::JSObject>>&&, std::optional<JSC::Strong<JSC::JSObject>>&&);
    static ExceptionOr<Ref<ReadableStream>> create(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    static ExceptionOr<Ref<ReadableStream>> createFromByteUnderlyingSource(JSDOMGlobalObject&, JSC::JSValue underlyingSource, UnderlyingSource&&, double highWaterMark);
    static Ref<ReadableStream> create(Ref<InternalReadableStream>&&);

    virtual ~ReadableStream();

    Ref<DOMPromise> cancelForBindings(JSDOMGlobalObject&, JSC::JSValue);
    ExceptionOr<ReadableStreamReader> getReader(JSDOMGlobalObject&, const GetReaderOptions&);
    ExceptionOr<Vector<Ref<ReadableStream>>> tee(JSDOMGlobalObject&, bool shouldClone = false);

    using State = InternalReadableStream::State;
    State state() const;

    void lock();
    bool isLocked() const;
    WEBCORE_EXPORT bool isDisturbed() const;

    Ref<DOMPromise> cancel(JSDOMGlobalObject&, JSC::JSValue);
    void cancel(Exception&&);

    InternalReadableStream* internalReadableStream() { return m_internalReadableStream.get(); }

    void setDefaultReader(ReadableStreamDefaultReader*);
    ReadableStreamDefaultReader* defaultReader();

    void pipeTo(ReadableStreamSink&);

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

    void visitAdditionalChildren(JSC::AbstractSlotVisitor&);

    static Ref<ReadableStream> createReadableByteStream(JSDOMGlobalObject&, ReadableByteStreamController::PullAlgorithm&&, ReadableByteStreamController::CancelAlgorithm&&, RefPtr<ReadableStream>&& = { });

    enum class Type : bool {
        Default,
        WebTransport
    };
    virtual Type type() const { return Type::Default; }

protected:
    static ExceptionOr<Ref<ReadableStream>> createFromJSValues(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue);
    static ExceptionOr<Ref<InternalReadableStream>> createInternalReadableStream(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    explicit ReadableStream(ScriptExecutionContext*, RefPtr<InternalReadableStream>&& = { }, RefPtr<ReadableStream>&& = { });

private:
    ExceptionOr<void> setupReadableByteStreamControllerFromUnderlyingSource(JSDOMGlobalObject&, JSC::JSValue, UnderlyingSource&&, double);
    void setupReadableByteStreamController(JSDOMGlobalObject&, ReadableByteStreamController::PullAlgorithm&&, ReadableByteStreamController::CancelAlgorithm&&, double);

    bool m_disturbed { false };
    WeakPtr<ReadableStreamDefaultReader> m_defaultReader;
    WeakPtr<ReadableStreamBYOBReader> m_byobReader;
    State m_state { State::Readable };

    const std::unique_ptr<ReadableByteStreamController> m_controller;
    const RefPtr<InternalReadableStream> m_internalReadableStream;

    const RefPtr<ReadableStream> m_relatedStreamForGC;
};

} // namespace WebCore
