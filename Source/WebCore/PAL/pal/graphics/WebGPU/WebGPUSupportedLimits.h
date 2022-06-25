/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <cstdint>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace PAL::WebGPU {

class SupportedLimits final : public RefCounted<SupportedLimits> {
public:
    static Ref<SupportedLimits> create(
        uint32_t maxTextureDimension1D,
        uint32_t maxTextureDimension2D,
        uint32_t maxTextureDimension3D,
        uint32_t maxTextureArrayLayers,
        uint32_t maxBindGroups,
        uint32_t maxDynamicUniformBuffersPerPipelineLayout,
        uint32_t maxDynamicStorageBuffersPerPipelineLayout,
        uint32_t maxSampledTexturesPerShaderStage,
        uint32_t maxSamplersPerShaderStage,
        uint32_t maxStorageBuffersPerShaderStage,
        uint32_t maxStorageTexturesPerShaderStage,
        uint32_t maxUniformBuffersPerShaderStage,
        uint64_t maxUniformBufferBindingSize,
        uint64_t maxStorageBufferBindingSize,
        uint32_t minUniformBufferOffsetAlignment,
        uint32_t minStorageBufferOffsetAlignment,
        uint32_t maxVertexBuffers,
        uint32_t maxVertexAttributes,
        uint32_t maxVertexBufferArrayStride,
        uint32_t maxInterStageShaderComponents,
        uint32_t maxComputeWorkgroupStorageSize,
        uint32_t maxComputeInvocationsPerWorkgroup,
        uint32_t maxComputeWorkgroupSizeX,
        uint32_t maxComputeWorkgroupSizeY,
        uint32_t maxComputeWorkgroupSizeZ,
        uint32_t maxComputeWorkgroupsPerDimension)
    {
        return adoptRef(*new SupportedLimits(
            maxTextureDimension1D,
            maxTextureDimension2D,
            maxTextureDimension3D,
            maxTextureArrayLayers,
            maxBindGroups,
            maxDynamicUniformBuffersPerPipelineLayout,
            maxDynamicStorageBuffersPerPipelineLayout,
            maxSampledTexturesPerShaderStage,
            maxSamplersPerShaderStage,
            maxStorageBuffersPerShaderStage,
            maxStorageTexturesPerShaderStage,
            maxUniformBuffersPerShaderStage,
            maxUniformBufferBindingSize,
            maxStorageBufferBindingSize,
            minUniformBufferOffsetAlignment,
            minStorageBufferOffsetAlignment,
            maxVertexBuffers,
            maxVertexAttributes,
            maxVertexBufferArrayStride,
            maxInterStageShaderComponents,
            maxComputeWorkgroupStorageSize,
            maxComputeInvocationsPerWorkgroup,
            maxComputeWorkgroupSizeX,
            maxComputeWorkgroupSizeY,
            maxComputeWorkgroupSizeZ,
            maxComputeWorkgroupsPerDimension));
    }

    static Ref<SupportedLimits> clone(const SupportedLimits& limits)
    {
        return adoptRef(*new SupportedLimits(
            limits.maxTextureDimension1D(),
            limits.maxTextureDimension2D(),
            limits.maxTextureDimension3D(),
            limits.maxTextureArrayLayers(),
            limits.maxBindGroups(),
            limits.maxDynamicUniformBuffersPerPipelineLayout(),
            limits.maxDynamicStorageBuffersPerPipelineLayout(),
            limits.maxSampledTexturesPerShaderStage(),
            limits.maxSamplersPerShaderStage(),
            limits.maxStorageBuffersPerShaderStage(),
            limits.maxStorageTexturesPerShaderStage(),
            limits.maxUniformBuffersPerShaderStage(),
            limits.maxUniformBufferBindingSize(),
            limits.maxStorageBufferBindingSize(),
            limits.minUniformBufferOffsetAlignment(),
            limits.minStorageBufferOffsetAlignment(),
            limits.maxVertexBuffers(),
            limits.maxVertexAttributes(),
            limits.maxVertexBufferArrayStride(),
            limits.maxInterStageShaderComponents(),
            limits.maxComputeWorkgroupStorageSize(),
            limits.maxComputeInvocationsPerWorkgroup(),
            limits.maxComputeWorkgroupSizeX(),
            limits.maxComputeWorkgroupSizeY(),
            limits.maxComputeWorkgroupSizeZ(),
            limits.maxComputeWorkgroupsPerDimension()));
    }

    uint32_t maxTextureDimension1D() const { return m_maxTextureDimension1D; }
    uint32_t maxTextureDimension2D() const { return m_maxTextureDimension2D; }
    uint32_t maxTextureDimension3D() const { return m_maxTextureDimension3D; }
    uint32_t maxTextureArrayLayers() const { return m_maxTextureArrayLayers; }
    uint32_t maxBindGroups() const { return m_maxBindGroups; }
    uint32_t maxDynamicUniformBuffersPerPipelineLayout() const { return m_maxDynamicUniformBuffersPerPipelineLayout; }
    uint32_t maxDynamicStorageBuffersPerPipelineLayout() const { return m_maxDynamicStorageBuffersPerPipelineLayout; }
    uint32_t maxSampledTexturesPerShaderStage() const { return m_maxSampledTexturesPerShaderStage; }
    uint32_t maxSamplersPerShaderStage() const { return m_maxSamplersPerShaderStage; }
    uint32_t maxStorageBuffersPerShaderStage() const { return m_maxStorageBuffersPerShaderStage; }
    uint32_t maxStorageTexturesPerShaderStage() const { return m_maxStorageTexturesPerShaderStage; }
    uint32_t maxUniformBuffersPerShaderStage() const { return m_maxUniformBuffersPerShaderStage; }
    uint64_t maxUniformBufferBindingSize() const { return m_maxUniformBufferBindingSize; }
    uint64_t maxStorageBufferBindingSize() const { return m_maxStorageBufferBindingSize; }
    uint32_t minUniformBufferOffsetAlignment() const { return m_minUniformBufferOffsetAlignment; }
    uint32_t minStorageBufferOffsetAlignment() const { return m_minStorageBufferOffsetAlignment; }
    uint32_t maxVertexBuffers() const { return m_maxVertexBuffers; }
    uint32_t maxVertexAttributes() const { return m_maxVertexAttributes; }
    uint32_t maxVertexBufferArrayStride() const { return m_maxVertexBufferArrayStride; }
    uint32_t maxInterStageShaderComponents() const { return m_maxInterStageShaderComponents; }
    uint32_t maxComputeWorkgroupStorageSize() const { return m_maxComputeWorkgroupStorageSize; }
    uint32_t maxComputeInvocationsPerWorkgroup() const { return m_maxComputeInvocationsPerWorkgroup; }
    uint32_t maxComputeWorkgroupSizeX() const { return m_maxComputeWorkgroupSizeX; }
    uint32_t maxComputeWorkgroupSizeY() const { return m_maxComputeWorkgroupSizeY; }
    uint32_t maxComputeWorkgroupSizeZ() const { return m_maxComputeWorkgroupSizeZ; }
    uint32_t maxComputeWorkgroupsPerDimension() const { return m_maxComputeWorkgroupsPerDimension; }

private:
    SupportedLimits(
        uint32_t maxTextureDimension1D,
        uint32_t maxTextureDimension2D,
        uint32_t maxTextureDimension3D,
        uint32_t maxTextureArrayLayers,
        uint32_t maxBindGroups,
        uint32_t maxDynamicUniformBuffersPerPipelineLayout,
        uint32_t maxDynamicStorageBuffersPerPipelineLayout,
        uint32_t maxSampledTexturesPerShaderStage,
        uint32_t maxSamplersPerShaderStage,
        uint32_t maxStorageBuffersPerShaderStage,
        uint32_t maxStorageTexturesPerShaderStage,
        uint32_t maxUniformBuffersPerShaderStage,
        uint64_t maxUniformBufferBindingSize,
        uint64_t maxStorageBufferBindingSize,
        uint32_t minUniformBufferOffsetAlignment,
        uint32_t minStorageBufferOffsetAlignment,
        uint32_t maxVertexBuffers,
        uint32_t maxVertexAttributes,
        uint32_t maxVertexBufferArrayStride,
        uint32_t maxInterStageShaderComponents,
        uint32_t maxComputeWorkgroupStorageSize,
        uint32_t maxComputeInvocationsPerWorkgroup,
        uint32_t maxComputeWorkgroupSizeX,
        uint32_t maxComputeWorkgroupSizeY,
        uint32_t maxComputeWorkgroupSizeZ,
        uint32_t maxComputeWorkgroupsPerDimension)
            : m_maxTextureDimension1D(maxTextureDimension1D)
            , m_maxTextureDimension2D(maxTextureDimension2D)
            , m_maxTextureDimension3D(maxTextureDimension3D)
            , m_maxTextureArrayLayers(maxTextureArrayLayers)
            , m_maxBindGroups(maxBindGroups)
            , m_maxDynamicUniformBuffersPerPipelineLayout(maxDynamicUniformBuffersPerPipelineLayout)
            , m_maxDynamicStorageBuffersPerPipelineLayout(maxDynamicStorageBuffersPerPipelineLayout)
            , m_maxSampledTexturesPerShaderStage(maxSampledTexturesPerShaderStage)
            , m_maxSamplersPerShaderStage(maxSamplersPerShaderStage)
            , m_maxStorageBuffersPerShaderStage(maxStorageBuffersPerShaderStage)
            , m_maxStorageTexturesPerShaderStage(maxStorageTexturesPerShaderStage)
            , m_maxUniformBuffersPerShaderStage(maxUniformBuffersPerShaderStage)
            , m_maxUniformBufferBindingSize(maxUniformBufferBindingSize)
            , m_maxStorageBufferBindingSize(maxStorageBufferBindingSize)
            , m_minUniformBufferOffsetAlignment(minUniformBufferOffsetAlignment)
            , m_minStorageBufferOffsetAlignment(minStorageBufferOffsetAlignment)
            , m_maxVertexBuffers(maxVertexBuffers)
            , m_maxVertexAttributes(maxVertexAttributes)
            , m_maxVertexBufferArrayStride(maxVertexBufferArrayStride)
            , m_maxInterStageShaderComponents(maxInterStageShaderComponents)
            , m_maxComputeWorkgroupStorageSize(maxComputeWorkgroupStorageSize)
            , m_maxComputeInvocationsPerWorkgroup(maxComputeInvocationsPerWorkgroup)
            , m_maxComputeWorkgroupSizeX(maxComputeWorkgroupSizeX)
            , m_maxComputeWorkgroupSizeY(maxComputeWorkgroupSizeY)
            , m_maxComputeWorkgroupSizeZ(maxComputeWorkgroupSizeZ)
            , m_maxComputeWorkgroupsPerDimension(maxComputeWorkgroupsPerDimension)
    {
    }

    SupportedLimits(const SupportedLimits&) = delete;
    SupportedLimits(SupportedLimits&&) = delete;
    SupportedLimits& operator=(const SupportedLimits&) = delete;
    SupportedLimits& operator=(SupportedLimits&&) = delete;

    uint32_t m_maxTextureDimension1D { 0 };
    uint32_t m_maxTextureDimension2D { 0 };
    uint32_t m_maxTextureDimension3D { 0 };
    uint32_t m_maxTextureArrayLayers { 0 };
    uint32_t m_maxBindGroups { 0 };
    uint32_t m_maxDynamicUniformBuffersPerPipelineLayout { 0 };
    uint32_t m_maxDynamicStorageBuffersPerPipelineLayout { 0 };
    uint32_t m_maxSampledTexturesPerShaderStage { 0 };
    uint32_t m_maxSamplersPerShaderStage { 0 };
    uint32_t m_maxStorageBuffersPerShaderStage { 0 };
    uint32_t m_maxStorageTexturesPerShaderStage { 0 };
    uint32_t m_maxUniformBuffersPerShaderStage { 0 };
    uint64_t m_maxUniformBufferBindingSize { 0 };
    uint64_t m_maxStorageBufferBindingSize { 0 };
    uint32_t m_minUniformBufferOffsetAlignment { 0 };
    uint32_t m_minStorageBufferOffsetAlignment { 0 };
    uint32_t m_maxVertexBuffers { 0 };
    uint32_t m_maxVertexAttributes { 0 };
    uint32_t m_maxVertexBufferArrayStride { 0 };
    uint32_t m_maxInterStageShaderComponents { 0 };
    uint32_t m_maxComputeWorkgroupStorageSize { 0 };
    uint32_t m_maxComputeInvocationsPerWorkgroup { 0 };
    uint32_t m_maxComputeWorkgroupSizeX { 0 };
    uint32_t m_maxComputeWorkgroupSizeY { 0 };
    uint32_t m_maxComputeWorkgroupSizeZ { 0 };
    uint32_t m_maxComputeWorkgroupsPerDimension { 0 };
};

} // namespace PAL::WebGPU
