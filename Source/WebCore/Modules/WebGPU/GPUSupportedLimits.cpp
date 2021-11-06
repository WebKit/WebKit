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

#include "config.h"
#include "GPUSupportedLimits.h"

namespace WebCore {

uint32_t GPUSupportedLimits::maxTextureDimension1D() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxTextureDimension2D() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxTextureDimension3D() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxTextureArrayLayers() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxBindGroups() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxDynamicUniformBuffersPerPipelineLayout() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxDynamicStorageBuffersPerPipelineLayout() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxSampledTexturesPerShaderStage() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxSamplersPerShaderStage() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxStorageBuffersPerShaderStage() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxStorageTexturesPerShaderStage() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxUniformBuffersPerShaderStage() const
{
    return 0;
}

uint64_t GPUSupportedLimits::maxUniformBufferBindingSize() const
{
    return 0;
}

uint64_t GPUSupportedLimits::maxStorageBufferBindingSize() const
{
    return 0;
}

uint32_t GPUSupportedLimits::minUniformBufferOffsetAlignment() const
{
    return 0;
}

uint32_t GPUSupportedLimits::minStorageBufferOffsetAlignment() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxVertexBuffers() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxVertexAttributes() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxVertexBufferArrayStride() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxInterStageShaderComponents() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxComputeWorkgroupStorageSize() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxComputeInvocationsPerWorkgroup() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxComputeWorkgroupSizeX() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxComputeWorkgroupSizeY() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxComputeWorkgroupSizeZ() const
{
    return 0;
}

uint32_t GPUSupportedLimits::maxComputeWorkgroupsPerDimension() const
{
    return 0;
}

}
