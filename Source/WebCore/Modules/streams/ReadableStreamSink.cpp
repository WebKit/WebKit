/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ReadableStreamSink.h"

#include "DOMException.h"
#include "JSDOMGlobalObject.h"
#include "ReadableStream.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/Uint8Array.h>

namespace WebCore {

ReadableStreamToSharedBufferSink::ReadableStreamToSharedBufferSink(Callback&& callback)
    : m_callback { WTFMove(callback) }
{
}

void ReadableStreamToSharedBufferSink::pipeFrom(ReadableStream& stream)
{
    stream.pipeTo(*this);
}

void ReadableStreamToSharedBufferSink::enqueue(const Ref<JSC::Uint8Array>& buffer)
{
    if (!buffer->byteLength())
        return;

    if (m_callback) {
        std::span<const uint8_t> chunk { buffer->data(), buffer->byteLength() };
        m_callback(&chunk);
    }
}

void ReadableStreamToSharedBufferSink::close()
{
    if (!m_callback)
        return;

    auto callback = std::exchange(m_callback, { });
    callback(nullptr);
}

void ReadableStreamToSharedBufferSink::error(String&& message)
{
    if (!m_callback)
        return;

    auto callback = std::exchange(m_callback, { });
    callback(Exception { ExceptionCode::TypeError, WTFMove(message) });
}

} // namespace WebCore
