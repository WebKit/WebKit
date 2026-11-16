/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
    static Ref<GPUSupportedLimits> create(Ref<WebGPUSupportedLimits>&& backing)
    {
        return adoptRef(*new GPUSupportedLimits(WTF::move(backing)));
    }

    NODELETE uint32_t maxTextureDimension1D() const;
    NODELETE uint32_t maxTextureDimension2D() const;
    NODELETE uint32_t maxTextureDimension3D() const;
    NODELETE uint32_t maxTextureArrayLayers() const;
    NODELETE uint32_t maxBindGroups() const;
    NODELETE uint32_t maxBindGroupsPlusVertexBuffers() const;
    NODELETE uint32_t maxBindingsPerBindGroup() const;
    NODELETE uint32_t maxDynamicUniformBuffersPerPipelineLayout() const;
    NODELETE uint32_t maxDynamicStorageBuffersPerPipelineLayout() const;
    NODELETE uint32_t maxSampledTexturesPerShaderStage() const;
    NODELETE uint32_t maxSamplersPerShaderStage() const;
    NODELETE uint32_t maxStorageBuffersPerShaderStage() const;
    NODELETE uint32_t maxStorageTexturesPerShaderStage() const;
    NODELETE uint32_t maxUniformBuffersPerShaderStage() const;
    NODELETE uint64_t maxUniformBufferBindingSize() const;
    NODELETE uint64_t maxStorageBufferBindingSize() const;
    NODELETE uint32_t minUniformBufferOffsetAlignment() const;
    NODELETE uint32_t minStorageBufferOffsetAlignment() const;
    NODELETE uint32_t maxVertexBuffers() const;
    NODELETE uint64_t maxBufferSize() const;
    NODELETE uint32_t maxVertexAttributes() const;
    NODELETE uint32_t maxVertexBufferArrayStride() const;
    NODELETE uint32_t maxInterStageShaderVariables() const;
    NODELETE uint32_t maxColorAttachments() const;
    NODELETE uint32_t maxColorAttachmentBytesPerSample() const;
    NODELETE uint32_t maxComputeWorkgroupStorageSize() const;
    NODELETE uint32_t maxComputeInvocationsPerWorkgroup() const;
    NODELETE uint32_t maxComputeWorkgroupSizeX() const;
    NODELETE uint32_t maxComputeWorkgroupSizeY() const;
    NODELETE uint32_t maxComputeWorkgroupSizeZ() const;
    NODELETE uint32_t maxComputeWorkgroupsPerDimension() const;
    NODELETE uint32_t maxStorageBuffersInFragmentStage() const;
    NODELETE uint32_t maxStorageTexturesInFragmentStage() const;
    NODELETE uint32_t maxStorageBuffersInVertexStage() const;
    NODELETE uint32_t maxStorageTexturesInVertexStage() const;

    WebGPUSupportedLimits& backing() { return m_backing; }
    const WebGPUSupportedLimits& backing() const { return m_backing; }

private:
    GPUSupportedLimits(Ref<WebGPUSupportedLimits>&& backing)
        : m_backing(WTF::move(backing))
    {
    }

    const Ref<WebGPUSupportedLimits> m_backing;
};

}
