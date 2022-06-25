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

#include "GPUColorDict.h"
#include "GPUIndexFormat.h"
#include "GPUIntegralTypes.h"
#include <JavaScriptCore/Uint32Array.h>
#include <optional>
#include <pal/graphics/WebGPU/WebGPURenderPassEncoder.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUBindGroup;
class GPUBuffer;
class GPUQuerySet;
class GPURenderBundle;
class GPURenderPipeline;

class GPURenderPassEncoder : public RefCounted<GPURenderPassEncoder> {
public:
    static Ref<GPURenderPassEncoder> create(Ref<PAL::WebGPU::RenderPassEncoder>&& backing)
    {
        return adoptRef(*new GPURenderPassEncoder(WTFMove(backing)));
    }

    String label() const;
    void setLabel(String&&);

    void setPipeline(const GPURenderPipeline&);

    void setIndexBuffer(const GPUBuffer&, GPUIndexFormat, GPUSize64 offset, std::optional<GPUSize64>);
    void setVertexBuffer(GPUIndex32 slot, const GPUBuffer&, GPUSize64 offset, std::optional<GPUSize64>);

    void draw(GPUSize32 vertexCount, GPUSize32 instanceCount,
        GPUSize32 firstVertex, GPUSize32 firstInstance);
    void drawIndexed(GPUSize32 indexCount, GPUSize32 instanceCount,
        GPUSize32 firstIndex,
        GPUSignedOffset32 baseVertex,
        GPUSize32 firstInstance);

    void drawIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset);
    void drawIndexedIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset);

    void setBindGroup(GPUIndex32, const GPUBindGroup&,
        std::optional<Vector<GPUBufferDynamicOffset>>&& dynamicOffsets);

    void setBindGroup(GPUIndex32, const GPUBindGroup&,
        const Uint32Array& dynamicOffsetsData,
        GPUSize64 dynamicOffsetsDataStart,
        GPUSize32 dynamicOffsetsDataLength);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void setViewport(float x, float y,
        float width, float height,
        float minDepth, float maxDepth);

    void setScissorRect(GPUIntegerCoordinate x, GPUIntegerCoordinate y,
        GPUIntegerCoordinate width, GPUIntegerCoordinate height);

    void setBlendConstant(GPUColor);
    void setStencilReference(GPUStencilValue);

    void beginOcclusionQuery(GPUSize32 queryIndex);
    void endOcclusionQuery();

    void executeBundles(Vector<RefPtr<GPURenderBundle>>&&);
    void end();

    PAL::WebGPU::RenderPassEncoder& backing() { return m_backing; }
    const PAL::WebGPU::RenderPassEncoder& backing() const { return m_backing; }

private:
    GPURenderPassEncoder(Ref<PAL::WebGPU::RenderPassEncoder>&& backing)
        : m_backing(WTFMove(backing))
    {
    }

    Ref<PAL::WebGPU::RenderPassEncoder> m_backing;
};

}
