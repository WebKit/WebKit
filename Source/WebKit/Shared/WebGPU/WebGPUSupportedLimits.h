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

#if ENABLE(GPU_PROCESS)

#include <cstdint>
#include <optional>

namespace WebKit::WebGPU {

struct SupportedLimits {
    uint32_t maxTextureDimension1D { 0 };
    uint32_t maxTextureDimension2D { 0 };
    uint32_t maxTextureDimension3D { 0 };
    uint32_t maxTextureArrayLayers { 0 };
    uint32_t maxBindGroups { 0 };
    uint32_t maxDynamicUniformBuffersPerPipelineLayout { 0 };
    uint32_t maxDynamicStorageBuffersPerPipelineLayout { 0 };
    uint32_t maxSampledTexturesPerShaderStage { 0 };
    uint32_t maxSamplersPerShaderStage { 0 };
    uint32_t maxStorageBuffersPerShaderStage { 0 };
    uint32_t maxStorageTexturesPerShaderStage { 0 };
    uint32_t maxUniformBuffersPerShaderStage { 0 };
    uint64_t maxUniformBufferBindingSize { 0 };
    uint64_t maxStorageBufferBindingSize { 0 };
    uint32_t minUniformBufferOffsetAlignment { 0 };
    uint32_t minStorageBufferOffsetAlignment { 0 };
    uint32_t maxVertexBuffers { 0 };
    uint32_t maxVertexAttributes { 0 };
    uint32_t maxVertexBufferArrayStride { 0 };
    uint32_t maxInterStageShaderComponents { 0 };
    uint32_t maxComputeWorkgroupStorageSize { 0 };
    uint32_t maxComputeInvocationsPerWorkgroup { 0 };
    uint32_t maxComputeWorkgroupSizeX { 0 };
    uint32_t maxComputeWorkgroupSizeY { 0 };
    uint32_t maxComputeWorkgroupSizeZ { 0 };
    uint32_t maxComputeWorkgroupsPerDimension { 0 };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << maxTextureDimension1D;
        encoder << maxTextureDimension2D;
        encoder << maxTextureDimension3D;
        encoder << maxTextureArrayLayers;
        encoder << maxBindGroups;
        encoder << maxDynamicUniformBuffersPerPipelineLayout;
        encoder << maxDynamicStorageBuffersPerPipelineLayout;
        encoder << maxSampledTexturesPerShaderStage;
        encoder << maxSamplersPerShaderStage;
        encoder << maxStorageBuffersPerShaderStage;
        encoder << maxStorageTexturesPerShaderStage;
        encoder << maxUniformBuffersPerShaderStage;
        encoder << maxUniformBufferBindingSize;
        encoder << maxStorageBufferBindingSize;
        encoder << minUniformBufferOffsetAlignment;
        encoder << minStorageBufferOffsetAlignment;
        encoder << maxVertexBuffers;
        encoder << maxVertexAttributes;
        encoder << maxVertexBufferArrayStride;
        encoder << maxInterStageShaderComponents;
        encoder << maxComputeWorkgroupStorageSize;
        encoder << maxComputeInvocationsPerWorkgroup;
        encoder << maxComputeWorkgroupSizeX;
        encoder << maxComputeWorkgroupSizeY;
        encoder << maxComputeWorkgroupSizeZ;
        encoder << maxComputeWorkgroupsPerDimension;
    }

    template<class Decoder> static std::optional<SupportedLimits> decode(Decoder& decoder)
    {
        std::optional<uint32_t> maxTextureDimension1D;
        decoder >> maxTextureDimension1D;
        if (!maxTextureDimension1D)
            return std::nullopt;

        std::optional<uint32_t> maxTextureDimension2D;
        decoder >> maxTextureDimension2D;
        if (!maxTextureDimension2D)
            return std::nullopt;

        std::optional<uint32_t> maxTextureDimension3D;
        decoder >> maxTextureDimension3D;
        if (!maxTextureDimension3D)
            return std::nullopt;

        std::optional<uint32_t> maxTextureArrayLayers;
        decoder >> maxTextureArrayLayers;
        if (!maxTextureArrayLayers)
            return std::nullopt;

        std::optional<uint32_t> maxBindGroups;
        decoder >> maxBindGroups;
        if (!maxBindGroups)
            return std::nullopt;

        std::optional<uint32_t> maxDynamicUniformBuffersPerPipelineLayout;
        decoder >> maxDynamicUniformBuffersPerPipelineLayout;
        if (!maxDynamicUniformBuffersPerPipelineLayout)
            return std::nullopt;

        std::optional<uint32_t> maxDynamicStorageBuffersPerPipelineLayout;
        decoder >> maxDynamicStorageBuffersPerPipelineLayout;
        if (!maxDynamicStorageBuffersPerPipelineLayout)
            return std::nullopt;

        std::optional<uint32_t> maxSampledTexturesPerShaderStage;
        decoder >> maxSampledTexturesPerShaderStage;
        if (!maxSampledTexturesPerShaderStage)
            return std::nullopt;

        std::optional<uint32_t> maxSamplersPerShaderStage;
        decoder >> maxSamplersPerShaderStage;
        if (!maxSamplersPerShaderStage)
            return std::nullopt;

        std::optional<uint32_t> maxStorageBuffersPerShaderStage;
        decoder >> maxStorageBuffersPerShaderStage;
        if (!maxStorageBuffersPerShaderStage)
            return std::nullopt;

        std::optional<uint32_t> maxStorageTexturesPerShaderStage;
        decoder >> maxStorageTexturesPerShaderStage;
        if (!maxStorageTexturesPerShaderStage)
            return std::nullopt;

        std::optional<uint32_t> maxUniformBuffersPerShaderStage;
        decoder >> maxUniformBuffersPerShaderStage;
        if (!maxUniformBuffersPerShaderStage)
            return std::nullopt;

        std::optional<uint64_t> maxUniformBufferBindingSize;
        decoder >> maxUniformBufferBindingSize;
        if (!maxUniformBufferBindingSize)
            return std::nullopt;

        std::optional<uint64_t> maxStorageBufferBindingSize;
        decoder >> maxStorageBufferBindingSize;
        if (!maxStorageBufferBindingSize)
            return std::nullopt;

        std::optional<uint32_t> minUniformBufferOffsetAlignment;
        decoder >> minUniformBufferOffsetAlignment;
        if (!minUniformBufferOffsetAlignment)
            return std::nullopt;

        std::optional<uint32_t> minStorageBufferOffsetAlignment;
        decoder >> minStorageBufferOffsetAlignment;
        if (!minStorageBufferOffsetAlignment)
            return std::nullopt;

        std::optional<uint32_t> maxVertexBuffers;
        decoder >> maxVertexBuffers;
        if (!maxVertexBuffers)
            return std::nullopt;

        std::optional<uint32_t> maxVertexAttributes;
        decoder >> maxVertexAttributes;
        if (!maxVertexAttributes)
            return std::nullopt;

        std::optional<uint32_t> maxVertexBufferArrayStride;
        decoder >> maxVertexBufferArrayStride;
        if (!maxVertexBufferArrayStride)
            return std::nullopt;

        std::optional<uint32_t> maxInterStageShaderComponents;
        decoder >> maxInterStageShaderComponents;
        if (!maxInterStageShaderComponents)
            return std::nullopt;

        std::optional<uint32_t> maxComputeWorkgroupStorageSize;
        decoder >> maxComputeWorkgroupStorageSize;
        if (!maxComputeWorkgroupStorageSize)
            return std::nullopt;

        std::optional<uint32_t> maxComputeInvocationsPerWorkgroup;
        decoder >> maxComputeInvocationsPerWorkgroup;
        if (!maxComputeInvocationsPerWorkgroup)
            return std::nullopt;

        std::optional<uint32_t> maxComputeWorkgroupSizeX;
        decoder >> maxComputeWorkgroupSizeX;
        if (!maxComputeWorkgroupSizeX)
            return std::nullopt;

        std::optional<uint32_t> maxComputeWorkgroupSizeY;
        decoder >> maxComputeWorkgroupSizeY;
        if (!maxComputeWorkgroupSizeY)
            return std::nullopt;

        std::optional<uint32_t> maxComputeWorkgroupSizeZ;
        decoder >> maxComputeWorkgroupSizeZ;
        if (!maxComputeWorkgroupSizeZ)
            return std::nullopt;

        std::optional<uint32_t> maxComputeWorkgroupsPerDimension;
        decoder >> maxComputeWorkgroupsPerDimension;
        if (!maxComputeWorkgroupsPerDimension)
            return std::nullopt;

        return { {
            *maxTextureDimension1D,
            *maxTextureDimension2D,
            *maxTextureDimension3D,
            *maxTextureArrayLayers,
            *maxBindGroups,
            *maxDynamicUniformBuffersPerPipelineLayout,
            *maxDynamicStorageBuffersPerPipelineLayout,
            *maxSampledTexturesPerShaderStage,
            *maxSamplersPerShaderStage,
            *maxStorageBuffersPerShaderStage,
            *maxStorageTexturesPerShaderStage,
            *maxUniformBuffersPerShaderStage,
            *maxUniformBufferBindingSize,
            *maxStorageBufferBindingSize,
            *minUniformBufferOffsetAlignment,
            *minStorageBufferOffsetAlignment,
            *maxVertexBuffers,
            *maxVertexAttributes,
            *maxVertexBufferArrayStride,
            *maxInterStageShaderComponents,
            *maxComputeWorkgroupStorageSize,
            *maxComputeInvocationsPerWorkgroup,
            *maxComputeWorkgroupSizeX,
            *maxComputeWorkgroupSizeY,
            *maxComputeWorkgroupSizeZ,
            *maxComputeWorkgroupsPerDimension,
        } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
