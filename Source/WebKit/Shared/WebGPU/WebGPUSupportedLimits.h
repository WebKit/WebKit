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
    uint32_t maxBindGroupsPlusVertexBuffers { 0 };
    uint32_t maxBindingsPerBindGroup { 0 };
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
    uint64_t maxBufferSize { 0 };
    uint32_t maxVertexAttributes { 0 };
    uint32_t maxVertexBufferArrayStride { 0 };
    uint32_t maxInterStageShaderComponents { 0 };
    uint32_t maxInterStageShaderVariables { 0 };
    uint32_t maxColorAttachments { 0 };
    uint32_t maxColorAttachmentBytesPerSample { 0 };
    uint32_t maxComputeWorkgroupStorageSize { 0 };
    uint32_t maxComputeInvocationsPerWorkgroup { 0 };
    uint32_t maxComputeWorkgroupSizeX { 0 };
    uint32_t maxComputeWorkgroupSizeY { 0 };
    uint32_t maxComputeWorkgroupSizeZ { 0 };
    uint32_t maxComputeWorkgroupsPerDimension { 0 };
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
