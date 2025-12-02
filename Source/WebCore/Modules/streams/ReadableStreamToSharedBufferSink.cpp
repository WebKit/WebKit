/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include "ReadableStreamToSharedBufferSink.h"

#include "DOMException.h"
#include "ExceptionOr.h"
#include "JSDOMConvertBufferSource.h"
#include "JSDOMGlobalObject.h"
#include "ReadableStream.h"
#include "ReadableStreamDefaultReader.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"

namespace WebCore {

class SinkReadRequest : public ReadableStreamReadRequest {
public:
    static Ref<SinkReadRequest> create(ReadableStreamToSharedBufferSink& sink, ScriptExecutionContext& context) { return adoptRef(*new SinkReadRequest(sink, context)); }
    ~SinkReadRequest() = default;

    JSDOMGlobalObject* globalObject() final
    {
        RefPtr context = m_context.get();
        return context ? JSC::jsCast<JSDOMGlobalObject*>(context->globalObject()): nullptr;
    }

private:
    SinkReadRequest(ReadableStreamToSharedBufferSink& sink, ScriptExecutionContext& context)
        : m_sink(sink)
        , m_context(context)
    {
    }

    void runChunkSteps(JSC::JSValue value) final
    {
        RefPtr sink = m_sink.get();
        if (!sink)
            return;

        auto* globalObject = this->globalObject();
        if (!globalObject)
            return;

        Ref vm = globalObject->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
        auto chunkOrException = convert<IDLUint8Array>(*globalObject, value);
        if (chunkOrException.hasException(scope)) [[unlikely]] {
            scope.clearException();
            sink->error(Exception { ExceptionCode::TypeError, "Unable to convert chunk to Uin8Array"_s });
            return;
        }

        sink->enqueue(chunkOrException.releaseReturnValue());
    }

    void runCloseSteps() final
    {
        if (RefPtr sink = m_sink.get())
            sink->close();
    }

    void runErrorSteps(JSC::JSValue value) final
    {
        if (RefPtr sink = m_sink.get())
            sink->error(value);
    }

    void runErrorSteps(Exception&& exception) final
    {
        if (RefPtr sink = m_sink.get())
            sink->error(WTFMove(exception));
    }

    WeakPtr<ReadableStreamToSharedBufferSink> m_sink;
    WeakPtr<ScriptExecutionContext> m_context;
};

ReadableStreamToSharedBufferSink::ReadableStreamToSharedBufferSink(Callback&& callback)
    : m_callback { WTFMove(callback) }
{
}

ReadableStreamToSharedBufferSink::~ReadableStreamToSharedBufferSink() = default;

void ReadableStreamToSharedBufferSink::pipeFrom(ReadableStream& stream)
{
    RefPtr context = stream.scriptExecutionContext();
    auto* globalObject = context ? JSC::jsCast<JSDOMGlobalObject*>(context->globalObject()): nullptr;
    if (!globalObject) {
        error(Exception { ExceptionCode::TypeError, "no global object"_s });
        return;
    }

    auto readerOrException = ReadableStreamDefaultReader::create(*globalObject, stream);
    if (readerOrException.hasException()) {
        error(readerOrException.releaseException());
        return;
    }

    m_reader = readerOrException.releaseReturnValue();
    m_readRequest = SinkReadRequest::create(*this, *context);

    Ref { *m_reader }->read(*globalObject, *m_readRequest);
}

void ReadableStreamToSharedBufferSink::enqueue(const Ref<JSC::Uint8Array>& buffer)
{
    if (buffer->byteLength()) {
        if (m_callback)
            m_callback(buffer->span());
    }

    RefPtr readRequest = m_readRequest;
    if (!readRequest)
        return;

    auto* globalObject = readRequest->globalObject();
    if (!globalObject)
        return;

    Ref { *m_reader }->read(*globalObject, readRequest.releaseNonNull());
}

void ReadableStreamToSharedBufferSink::close()
{
    m_reader = nullptr;
    m_readRequest = nullptr;
    if (!m_callback)
        return;

    auto callback = std::exchange(m_callback, { });
    callback(nullptr);
}

void ReadableStreamToSharedBufferSink::error(JSC::JSValue value)
{
    m_reader = nullptr;
    m_readRequest = nullptr;
    if (!m_callback)
        return;

    auto callback = std::exchange(m_callback, { });
    callback(value);
}

void ReadableStreamToSharedBufferSink::error(Exception&& exception)
{
    m_reader = nullptr;
    m_readRequest = nullptr;
    if (!m_callback)
        return;

    auto callback = std::exchange(m_callback, { });
    callback(WTFMove(exception));
}

void ReadableStreamToSharedBufferSink::clearCallback()
{
    m_reader = nullptr;
    m_readRequest = nullptr;
    m_callback = { };
}

} // namespace WebCore
