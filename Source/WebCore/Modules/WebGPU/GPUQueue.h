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

#include "BufferSource.h"
#include "GPUCommandBuffer.h"
#include "GPUExtent3DDict.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUImageCopyTexture.h"
#include "GPUImageCopyTextureTagged.h"
#include "GPUImageDataLayout.h"
#include "GPUIntegralTypes.h"
#include "JSDOMPromiseDeferred.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUQueue.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUBuffer;

class GPUQueue : public RefCounted<GPUQueue> {
public:
    static Ref<GPUQueue> create(Ref<PAL::WebGPU::Queue>&& backing)
    {
        return adoptRef(*new GPUQueue(WTFMove(backing)));
    }

    String label() const;
    void setLabel(String&&);

    void submit(Vector<RefPtr<GPUCommandBuffer>>&&);

    using OnSubmittedWorkDonePromise = DOMPromiseDeferred<IDLNull>;
    void onSubmittedWorkDone(OnSubmittedWorkDonePromise&&);

    void writeBuffer(
        const GPUBuffer&,
        GPUSize64 bufferOffset,
        BufferSource&& data,
        GPUSize64 dataOffset,
        std::optional<GPUSize64>);

    void writeTexture(
        const GPUImageCopyTexture& destination,
        BufferSource&& data,
        const GPUImageDataLayout&,
        const GPUExtent3D& size);

    void copyExternalImageToTexture(
        const GPUImageCopyExternalImage& source,
        const GPUImageCopyTextureTagged& destination,
        const GPUExtent3D& copySize);

    PAL::WebGPU::Queue& backing() { return m_backing; }
    const PAL::WebGPU::Queue& backing() const { return m_backing; }

private:
    GPUQueue(Ref<PAL::WebGPU::Queue>&& backing)
        : m_backing(WTFMove(backing))
    {
    }

    Ref<PAL::WebGPU::Queue> m_backing;
};

}
