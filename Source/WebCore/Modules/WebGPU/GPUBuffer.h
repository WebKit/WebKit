/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "GPUIntegralTypes.h"
#include "GPUMapMode.h"
#include "JSDOMPromiseDeferred.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <cstdint>
#include <optional>
#include <pal/graphics/WebGPU/WebGPUBuffer.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUBuffer : public RefCounted<GPUBuffer> {
public:
    static Ref<GPUBuffer> create(Ref<PAL::WebGPU::Buffer>&& backing)
    {
        return adoptRef(*new GPUBuffer(WTFMove(backing)));
    }

    String label() const;
    void setLabel(String&&);

    using MapAsyncPromise = DOMPromiseDeferred<IDLNull>;
    void mapAsync(GPUMapModeFlags, std::optional<GPUSize64> offset, std::optional<GPUSize64> sizeForMap, MapAsyncPromise&&);
    Ref<JSC::ArrayBuffer> getMappedRange(std::optional<GPUSize64> offset, std::optional<GPUSize64> rangeSize);
    void unmap();

    void destroy();

    PAL::WebGPU::Buffer& backing() { return m_backing; }
    const PAL::WebGPU::Buffer& backing() const { return m_backing; }

    ~GPUBuffer() { destroy(); }
private:
    GPUBuffer(Ref<PAL::WebGPU::Buffer>&& backing)
        : m_backing(WTFMove(backing))
    {
    }

    Ref<PAL::WebGPU::Buffer> m_backing;
    PAL::WebGPU::Buffer::MappedRange m_mappedRange;
    JSC::ArrayBuffer* m_arrayBuffer { nullptr };
};

}
