/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WritableStream.h"

#include "InternalWritableStream.h"
#include "JSDOMGlobalObject.h"
#include "JSWritableStream.h"
#include "JSWritableStreamSink.h"
#include "MessageChannel.h"
#include "MessagePort.h"
#include "ReadableStream.h"
#include "Settings.h"
#include "StreamPipeOptions.h"
#include "StreamPipeToUtilities.h"
#include "StreamTransferUtilities.h"

namespace WebCore {

ExceptionOr<Ref<WritableStream>> WritableStream::create(JSC::JSGlobalObject& globalObject, JSC::Strong<JSC::JSObject>&& underlyingSink, JSC::Strong<JSC::JSObject>&& strategy)
{
    JSC::JSValue underlyingSinkValue = JSC::jsUndefined();
    if (underlyingSink)
        underlyingSinkValue = underlyingSink.get();

    JSC::JSValue strategyValue = JSC::jsUndefined();
    if (strategy)
        strategyValue = strategy.get();

    return create(globalObject, underlyingSinkValue, strategyValue);
}

WritableStream::~WritableStream() = default;

void WritableStream::lock()
{
    m_internalWritableStream->lock();
}

bool WritableStream::locked() const
{
    return m_internalWritableStream->locked();
}

InternalWritableStream& WritableStream::internalWritableStream()
{
    return m_internalWritableStream.get();
}

ExceptionOr<Ref<InternalWritableStream>> WritableStream::createInternalWritableStream(JSDOMGlobalObject& globalObject, Ref<WritableStreamSink>&& sink)
{
    return InternalWritableStream::createFromUnderlyingSink(globalObject, toJSNewlyCreated(&globalObject, &globalObject, WTF::move(sink)), JSC::jsUndefined());
}

ExceptionOr<Ref<WritableStream>> WritableStream::create(JSC::JSGlobalObject& globalObject, JSC::JSValue underlyingSink, JSC::JSValue strategy)
{
    auto result = InternalWritableStream::createFromUnderlyingSink(*JSC::jsCast<JSDOMGlobalObject*>(&globalObject), underlyingSink, strategy);
    if (result.hasException())
        return result.releaseException();

    return adoptRef(*new WritableStream(result.releaseReturnValue()));
}

ExceptionOr<Ref<WritableStream>> WritableStream::create(JSDOMGlobalObject& globalObject, Ref<WritableStreamSink>&& sink)
{
    return create(globalObject, toJSNewlyCreated(&globalObject, &globalObject, WTF::move(sink)), JSC::jsUndefined());
}

Ref<WritableStream> WritableStream::create(Ref<InternalWritableStream>&& internalWritableStream)
{
    return adoptRef(*new WritableStream(WTF::move(internalWritableStream)));
}

WritableStream::WritableStream(Ref<InternalWritableStream>&& internalWritableStream)
    : m_internalWritableStream(WTF::move(internalWritableStream))
{
}

void WritableStream::closeIfPossible()
{
    m_internalWritableStream->closeIfPossible();
}

void WritableStream::errorIfPossible(Exception&& e)
{
    m_internalWritableStream->errorIfPossible(WTF::move(e));
}

void WritableStream::errorIfPossible(JSC::JSGlobalObject& globalObject, JSC::JSValue reason)
{
    m_internalWritableStream->errorIfPossible(globalObject, reason);
}

WritableStream::State WritableStream::state() const
{
    auto* globalObject = m_internalWritableStream->globalObject();
    if (!globalObject)
        return State::Errored;

    auto state = m_internalWritableStream->state(*globalObject);
    if (state == "writable"_s)
        return State::Writable;
    if (state == "closed"_s)
        return State::Closed;
    return State::Errored;
}

// https://streams.spec.whatwg.org/#ws-transfer
bool WritableStream::canTransfer() const
{
    auto* globalObject = m_internalWritableStream->globalObject();
    RefPtr context = globalObject ? globalObject->scriptExecutionContext() : nullptr;
    return context && context->settingsValues().readableStreamTransferEnabled && !locked();
}

ExceptionOr<DetachedWritableStream> WritableStream::runTransferSteps(JSDOMGlobalObject& globalObject)
{
    ASSERT(canTransfer());

    RefPtr context = globalObject.scriptExecutionContext();
    Ref channel = MessageChannel::create(*context);
    Ref port1 = channel->port1();
    Ref port2 = channel->port2();

    auto result = setupCrossRealmTransformReadable(globalObject, port1.get());
    if (result.hasException()) {
        port2->close();
        return result.releaseException();
    }
    Ref readable = result.releaseReturnValue();

    if (auto exception = readableStreamPipeTo(globalObject, readable, *this, { }, nullptr)) {
        port2->close();
        return WTF::move(*exception);
    }

    return DetachedWritableStream { WTF::move(port2) };
}

ExceptionOr<Ref<WritableStream>> WritableStream::runTransferReceivingSteps(JSDOMGlobalObject& globalObject, DetachedWritableStream&& detachedWritableStream)
{
    return setupCrossRealmTransformWritable(globalObject, detachedWritableStream.writableStreamPort.get());
}

JSC::JSValue JSWritableStream::abort(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    return wrapped().internalWritableStream().abortForBindings(globalObject, callFrame.argument(0));
}

JSC::JSValue JSWritableStream::close(JSC::JSGlobalObject& globalObject, JSC::CallFrame&)
{
    return wrapped().internalWritableStream().closeForBindings(globalObject);
}

JSC::JSValue JSWritableStream::getWriter(JSC::JSGlobalObject& globalObject, JSC::CallFrame&)
{
    return wrapped().internalWritableStream().getWriter(globalObject);
}

} // namespace WebCore
