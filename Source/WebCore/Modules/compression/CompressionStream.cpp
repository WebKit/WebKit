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
#include "CompressionStream.h"

#include "ScriptExecutionContext.h"
#include "WritableStreamSink.h"
#include <wtf/IsoMalloc.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CompressionStream);

ExceptionOr<void> CompressionStream::createStreams()
{
    // get global object
    auto* globalObject = scriptExecutionContext() ? scriptExecutionContext()->globalObject() : nullptr;
    if (!globalObject)
        return Exception { InvalidStateError };

    // setup readable stream
    m_readableStreamSource = SimpleReadableStreamSource::create();
    auto readable = ReadableStream::create(*globalObject, m_readableStreamSource.copyRef());
    if (readable.hasException())
        return readable.releaseException();
    
    m_readable = readable.releaseReturnValue();

    // setup writable stream
    auto simpleWritableStreamSink = SimpleWritableStreamSink::create([readableStreamSource = m_readableStreamSource, weakThis = WeakPtr { *this }](auto& context, auto value) -> ExceptionOr<void> {
        if (!context.globalObject())
            return Exception { InvalidStateError };

        UNUSED_PARAM(value);
        return { };
    });
    auto writable = WritableStream::create(*JSC::jsCast<JSDOMGlobalObject*>(globalObject), simpleWritableStreamSink);
    
    m_writable = writable.releaseReturnValue();

    return { };
}

ExceptionOr<RefPtr<ReadableStream>> CompressionStream::readable()
{
    if (!m_readable) {
        auto result = createStreams();
        if (result.hasException())
            return result.releaseException();
    }

    return m_readable.copyRef();
}

ExceptionOr<RefPtr<WritableStream>> CompressionStream::writable()
{
    if (!m_writable) {
        auto result = createStreams();
        if (result.hasException())
            return result.releaseException();
    }

    return m_writable.copyRef();
}

}
