/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebGPUBuffer.h"

#if ENABLE(WEBGPU)

#include "GPUErrorScopes.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

Ref<WebGPUBuffer> WebGPUBuffer::create(RefPtr<GPUBuffer>&& buffer, GPUErrorScopes& errorScopes)
{
    return adoptRef(*new WebGPUBuffer(WTFMove(buffer), errorScopes));
}

WebGPUBuffer::WebGPUBuffer(RefPtr<GPUBuffer>&& buffer, GPUErrorScopes& errorScopes)
    : GPUObjectBase(makeRef(errorScopes))
    , m_buffer(WTFMove(buffer))
{
}

void WebGPUBuffer::mapReadAsync(BufferMappingPromise&& promise)
{
    rejectOrRegisterPromiseCallback(WTFMove(promise), true);
}

void WebGPUBuffer::mapWriteAsync(BufferMappingPromise&& promise)
{
    rejectOrRegisterPromiseCallback(WTFMove(promise), false);
}

void WebGPUBuffer::unmap()
{
    errorScopes().setErrorPrefix("GPUBuffer.unmap(): ");

    if (!m_buffer)
        errorScopes().generatePrefixedError("Invalid operation: invalid GPUBuffer!");
    else
        m_buffer->unmap(&errorScopes());
}

void WebGPUBuffer::destroy()
{
    errorScopes().setErrorPrefix("GPUBuffer.destroy(): ");

    if (!m_buffer)
        errorScopes().generatePrefixedError("Invalid operation!");
    else {
        m_buffer->destroy(&errorScopes());
        m_buffer = nullptr;
    }
}

void WebGPUBuffer::rejectOrRegisterPromiseCallback(BufferMappingPromise&& promise, bool isRead)
{
    errorScopes().setErrorPrefix(makeString("GPUBuffer.map", isRead ? "Read" : "Write", "Async(): "));

    if (!m_buffer) {
        errorScopes().generatePrefixedError("Invalid operation: invalid GPUBuffer!");
        promise.reject();
        return;
    }

    m_buffer->registerMappingCallback([promise = WTFMove(promise), protectedErrorScopes = makeRef(errorScopes())] (JSC::ArrayBuffer* arrayBuffer) mutable {
        if (arrayBuffer)
            promise.resolve(*arrayBuffer);
        else {
            // FIXME: It's possible to hit this code path in response to an unmap call, where
            // the ArrayBuffer will definitely be null, and thus an out of memory error makes no sense.
            // https://bugs.webkit.org/show_bug.cgi?id=204166
            protectedErrorScopes->generateError("", GPUErrorFilter::OutOfMemory);
            promise.reject();
        }
    }, isRead, errorScopes());
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
