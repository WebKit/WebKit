/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ReadableStreamDefaultReader.h"

#include "JSReadableStreamDefaultReader.h"
#include "ReadableStream.h"

namespace WebCore {

ExceptionOr<Ref<ReadableStreamDefaultReader>> ReadableStreamDefaultReader::create(JSDOMGlobalObject& globalObject, ReadableStream& stream)
{
    Ref internalReadableStream = stream.internalReadableStream();
    auto internalReaderOrException = InternalReadableStreamDefaultReader::create(globalObject, internalReadableStream.get());
    if (internalReaderOrException.hasException())
        return internalReaderOrException.releaseException();

    return create(internalReaderOrException.releaseReturnValue());
}

Ref<ReadableStreamDefaultReader> ReadableStreamDefaultReader::create(Ref<InternalReadableStreamDefaultReader>&& internalDefaultReader)
{
    return adoptRef(*new ReadableStreamDefaultReader(WTFMove(internalDefaultReader)));
}

ReadableStreamDefaultReader::ReadableStreamDefaultReader(Ref<InternalReadableStreamDefaultReader>&& internalDefaultReader)
    : m_internalDefaultReader(WTFMove(internalDefaultReader))
{
}

ExceptionOr<void> ReadableStreamDefaultReader::releaseLock()
{
    Ref internalDefaultReader = this->internalDefaultReader();
    return internalDefaultReader->releaseLock();
}

JSC::JSValue JSReadableStreamDefaultReader::read(JSC::JSGlobalObject& globalObject, JSC::CallFrame&)
{
    Ref internalDefaultReader = wrapped().internalDefaultReader();
    return internalDefaultReader->readForBindings(globalObject);
}

JSC::JSValue JSReadableStreamDefaultReader::closed(JSC::JSGlobalObject& globalObject) const
{
    Ref internalDefaultReader = wrapped().internalDefaultReader();
    return internalDefaultReader->closedForBindings(globalObject);
}

JSC::JSValue JSReadableStreamDefaultReader::cancel(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    Ref internalDefaultReader = wrapped().internalDefaultReader();
    return internalDefaultReader->cancelForBindings(globalObject, callFrame.argument(0));
}

} // namespace WebCore
