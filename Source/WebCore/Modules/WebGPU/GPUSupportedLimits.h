/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "WebGPUSupportedLimits.h"

namespace WebCore {

class GPUSupportedLimits : public RefCounted<GPUSupportedLimits> {
public:
    static Ref<GPUSupportedLimits> create(Ref<WebGPU::SupportedLimits>&& backing)
    {
        return adoptRef(*new GPUSupportedLimits(WTFMove(backing)));
    }

    uint32_t maxTextureDimension1D() const;
    uint32_t maxTextureDimension2D() const;
    uint32_t maxTextureDimension3D() const;
    uint32_t maxTextureArrayLayers() const;
    uint32_t maxBindGroups() const;
    uint32_t maxBindGroupsPlusVertexBuffers() const;
    uint32_t maxBindingsPerBindGroup() const;
    uint32_t maxDynamicUniformBuffersPerPipelineLayout() const;
    uint32_t maxDynamicStorageBuffersPerPipelineLayout() const;
    uint32_t maxSampledTexturesPerShaderStage() const;
    uint32_t maxSamplersPerShaderStage() const;
    uint32_t maxStorageBuffersPerShaderStage() const;
    uint32_t maxStorageTexturesPerShaderStage() const;
    uint32_t maxUniformBuffersPerShaderStage() const;
    uint64_t maxUniformBufferBindingSize() const;
    uint64_t maxStorageBufferBindingSize() const;
    uint32_t minUniformBufferOffsetAlignment() const;
    uint32_t minStorageBufferOffsetAlignment() const;
    uint32_t maxVertexBuffers() const;
    uint64_t maxBufferSize() const;
    uint32_t maxVertexAttributes() const;
    uint32_t maxVertexBufferArrayStride() const;
    uint32_t maxInterStageShaderComponents() const;
    uint32_t maxInterStageShaderVariables() const;
    uint32_t maxColorAttachments() const;
    uint32_t maxColorAttachmentBytesPerSample() const;
    uint32_t maxComputeWorkgroupStorageSize() const;
    uint32_t maxComputeInvocationsPerWorkgroup() const;
    uint32_t maxComputeWorkgroupSizeX() const;
    uint32_t maxComputeWorkgroupSizeY() const;
    uint32_t maxComputeWorkgroupSizeZ() const;
    uint32_t maxComputeWorkgroupsPerDimension() const;

    WebGPU::SupportedLimits& backing() { return m_backing; }
    const WebGPU::SupportedLimits& backing() const { return m_backing; }

private:
    GPUSupportedLimits(Ref<WebGPU::SupportedLimits>&& backing)
        : m_backing(WTFMove(backing))
    {
    }

    Ref<WebGPU::SupportedLimits> m_backing;
};

}
