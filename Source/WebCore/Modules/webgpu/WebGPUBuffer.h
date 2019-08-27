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

#pragma once

#if ENABLE(WEBGPU)

#include "GPUBuffer.h"
#include "GPUBufferUsage.h"
#include "GPUObjectBase.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/RefPtr.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

struct GPUBufferDescriptor;

class WebGPUBuffer : public GPUObjectBase {
public:
    static Ref<WebGPUBuffer> create(RefPtr<GPUBuffer>&&, GPUErrorScopes&);

    GPUBuffer* buffer() { return m_buffer.get(); }
    const GPUBuffer* buffer() const { return m_buffer.get(); }

    using BufferMappingPromise = DOMPromiseDeferred<IDLInterface<JSC::ArrayBuffer>>;
    void mapReadAsync(BufferMappingPromise&&);
    void mapWriteAsync(BufferMappingPromise&&);
    void unmap();
    void destroy();

private:
    explicit WebGPUBuffer(RefPtr<GPUBuffer>&&, GPUErrorScopes&);

    void rejectOrRegisterPromiseCallback(BufferMappingPromise&&, bool);

    RefPtr<GPUBuffer> m_buffer;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
