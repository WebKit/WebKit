/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ReadableStream.h"

#include "JSReadableStream.h"
#include "JSReadableStreamSource.h"

namespace WebCore {

ExceptionOr<Ref<ReadableStream>> ReadableStream::create(JSC::JSGlobalObject& globalObject, std::optional<JSC::Strong<JSC::JSObject>>&& underlyingSource, std::optional<JSC::Strong<JSC::JSObject>>&& strategy)
{
    JSC::JSValue underlyingSourceValue = JSC::jsUndefined();
    if (underlyingSource)
        underlyingSourceValue = underlyingSource->get();

    JSC::JSValue strategyValue = JSC::jsUndefined();
    if (strategy)
        strategyValue = strategy->get();

    return createFromJSValues(globalObject, underlyingSourceValue, strategyValue);
}

ExceptionOr<Ref<ReadableStream>> ReadableStream::createFromJSValues(JSC::JSGlobalObject& globalObject, JSC::JSValue underlyingSource, JSC::JSValue strategy)
{
    auto result = InternalReadableStream::createFromUnderlyingSource(*JSC::jsCast<JSDOMGlobalObject*>(&globalObject), underlyingSource, strategy);
    if (result.hasException())
        return result.releaseException();

    return adoptRef(*new ReadableStream(result.releaseReturnValue()));
}

ExceptionOr<Ref<ReadableStream>> ReadableStream::create(JSDOMGlobalObject& globalObject, Ref<ReadableStreamSource>&& source)
{
    return createFromJSValues(globalObject, toJSNewlyCreated(&globalObject, &globalObject, WTFMove(source)), JSC::jsUndefined());
}

Ref<ReadableStream> ReadableStream::create(Ref<InternalReadableStream>&& internalReadableStream)
{
    return adoptRef(*new ReadableStream(WTFMove(internalReadableStream)));
}

ReadableStream::ReadableStream(Ref<InternalReadableStream>&& internalReadableStream)
    : m_internalReadableStream(WTFMove(internalReadableStream))
{
}

ExceptionOr<Vector<Ref<ReadableStream>>> ReadableStream::tee(bool shouldClone)
{
    auto result = m_internalReadableStream->tee(shouldClone);
    if (result.hasException())
        return result.releaseException();

    auto pair = result.releaseReturnValue();

    Vector<Ref<ReadableStream>> sequence;
    sequence.reserveInitialCapacity(2);
    sequence.uncheckedAppend(ReadableStream::create(WTFMove(pair.first)));
    sequence.uncheckedAppend(ReadableStream::create(WTFMove(pair.second)));
    return sequence;
}

JSC::JSValue JSReadableStream::cancel(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    return wrapped().internalReadableStream().cancelForBindings(globalObject, callFrame.argument(0));
}

JSC::JSValue JSReadableStream::getReader(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    return wrapped().internalReadableStream().getReader(globalObject, callFrame.argument(0));
}

JSC::JSValue JSReadableStream::pipeTo(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    return wrapped().internalReadableStream().pipeTo(globalObject, callFrame.argument(0), callFrame.argument(1));
}

JSC::JSValue JSReadableStream::pipeThrough(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    return wrapped().internalReadableStream().pipeThrough(globalObject, callFrame.argument(0), callFrame.argument(1));
}

} // namespace WebCore
