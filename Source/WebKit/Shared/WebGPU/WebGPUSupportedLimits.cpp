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

#include "config.h"
#include "WebGPUSupportedLimits.h"

#if ENABLE(GPU_PROCESS)


namespace WebKit::WebGPU {

SupportedLimits convertToBacking(const ::WebGPU::Limits& limits)
{
    return {
        limits.maxTextureDimension1D,
        limits.maxTextureDimension2D,
        limits.maxTextureDimension3D,
        limits.maxTextureArrayLayers,
        limits.maxBindGroups,
        limits.maxBindGroupsPlusVertexBuffers,
        limits.maxBindingsPerBindGroup,
        limits.maxDynamicUniformBuffersPerPipelineLayout,
        limits.maxDynamicStorageBuffersPerPipelineLayout,
        limits.maxSampledTexturesPerShaderStage,
        limits.maxSamplersPerShaderStage,
        limits.maxStorageBuffersPerShaderStage,
        limits.maxStorageTexturesPerShaderStage,
        limits.maxUniformBuffersPerShaderStage,
        limits.maxUniformBufferBindingSize,
        limits.maxStorageBufferBindingSize,
        limits.minUniformBufferOffsetAlignment,
        limits.minStorageBufferOffsetAlignment,
        limits.maxVertexBuffers,
        limits.maxBufferSize,
        limits.maxVertexAttributes,
        limits.maxVertexBufferArrayStride,
        limits.maxInterStageShaderVariables,
        limits.maxColorAttachments,
        limits.maxColorAttachmentBytesPerSample,
        limits.maxComputeWorkgroupStorageSize,
        limits.maxComputeInvocationsPerWorkgroup,
        limits.maxComputeWorkgroupSizeX,
        limits.maxComputeWorkgroupSizeY,
        limits.maxComputeWorkgroupSizeZ,
        limits.maxComputeWorkgroupsPerDimension,
        limits.maxStorageBuffersInFragmentStage,
        limits.maxStorageTexturesInFragmentStage,
        limits.maxStorageBuffersInVertexStage,
        limits.maxStorageTexturesInVertexStage,
    };
}

::WebGPU::Limits convertFromBacking(const SupportedLimits& limits)
{
    return {
        .maxTextureDimension1D = limits.maxTextureDimension1D,
        .maxTextureDimension2D = limits.maxTextureDimension2D,
        .maxTextureDimension3D = limits.maxTextureDimension3D,
        .maxTextureArrayLayers = limits.maxTextureArrayLayers,
        .maxBindGroups = limits.maxBindGroups,
        .maxBindGroupsPlusVertexBuffers = limits.maxBindGroupsPlusVertexBuffers,
        .maxBindingsPerBindGroup = limits.maxBindingsPerBindGroup,
        .maxDynamicUniformBuffersPerPipelineLayout = limits.maxDynamicUniformBuffersPerPipelineLayout,
        .maxDynamicStorageBuffersPerPipelineLayout = limits.maxDynamicStorageBuffersPerPipelineLayout,
        .maxSampledTexturesPerShaderStage = limits.maxSampledTexturesPerShaderStage,
        .maxSamplersPerShaderStage = limits.maxSamplersPerShaderStage,
        .maxStorageBuffersPerShaderStage = limits.maxStorageBuffersPerShaderStage,
        .maxStorageTexturesPerShaderStage = limits.maxStorageTexturesPerShaderStage,
        .maxUniformBuffersPerShaderStage = limits.maxUniformBuffersPerShaderStage,
        .maxUniformBufferBindingSize = limits.maxUniformBufferBindingSize,
        .maxStorageBufferBindingSize = limits.maxStorageBufferBindingSize,
        .minUniformBufferOffsetAlignment = limits.minUniformBufferOffsetAlignment,
        .minStorageBufferOffsetAlignment = limits.minStorageBufferOffsetAlignment,
        .maxVertexBuffers = limits.maxVertexBuffers,
        .maxBufferSize = limits.maxBufferSize,
        .maxVertexAttributes = limits.maxVertexAttributes,
        .maxVertexBufferArrayStride = limits.maxVertexBufferArrayStride,
        .maxInterStageShaderVariables = limits.maxInterStageShaderVariables,
        .maxColorAttachments = limits.maxColorAttachments,
        .maxColorAttachmentBytesPerSample = limits.maxColorAttachmentBytesPerSample,
        .maxComputeWorkgroupStorageSize = limits.maxComputeWorkgroupStorageSize,
        .maxComputeInvocationsPerWorkgroup = limits.maxComputeInvocationsPerWorkgroup,
        .maxComputeWorkgroupSizeX = limits.maxComputeWorkgroupSizeX,
        .maxComputeWorkgroupSizeY = limits.maxComputeWorkgroupSizeY,
        .maxComputeWorkgroupSizeZ = limits.maxComputeWorkgroupSizeZ,
        .maxComputeWorkgroupsPerDimension = limits.maxComputeWorkgroupsPerDimension,
        .maxStorageBuffersInFragmentStage = limits.maxStorageBuffersInFragmentStage,
        .maxStorageTexturesInFragmentStage = limits.maxStorageTexturesInFragmentStage,
        .maxStorageBuffersInVertexStage = limits.maxStorageBuffersInVertexStage,
        .maxStorageTexturesInVertexStage = limits.maxStorageTexturesInVertexStage,
    };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
