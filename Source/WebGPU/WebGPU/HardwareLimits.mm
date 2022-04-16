/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "HardwareLimits.h"

#import <algorithm>
#import <limits>
#import <wtf/MathExtras.h>
#import <wtf/PageBlock.h>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

// FIXME: https://github.com/gpuweb/gpuweb/issues/2749 we need more limits.

constexpr static WGPULimits apple3()
{
    uint32_t maxBindGroups = 30;

    return WGPULimits {
        /* maxTextureDimension1D */    16384,
        /* maxTextureDimension2D */    16384,
        /* maxTextureDimension3D */    2048,
        /* maxTextureArrayLayers */    2048,
        /* maxBindGroups */    maxBindGroups,
        /* maxDynamicUniformBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxDynamicStorageBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxSampledTexturesPerShaderStage */    maxBindGroups * 31,
        /* maxSamplersPerShaderStage */    maxBindGroups * 16,
        /* maxStorageBuffersPerShaderStage */    maxBindGroups * 31,
        /* maxStorageTexturesPerShaderStage */    maxBindGroups * 31,
        /* maxUniformBuffersPerShaderStage */    maxBindGroups * 31,
        /* maxUniformBufferBindingSize */    0, // To be filled in by the caller.
        /* maxStorageBufferBindingSize */    0, // To be filled in by the caller.
        /* minUniformBufferOffsetAlignment */    4,
        /* minStorageBufferOffsetAlignment */    4,
        /* maxVertexBuffers */    30,
        /* maxVertexAttributes */    31,
        /* maxVertexBufferArrayStride */    std::numeric_limits<uint32_t>::max(),
        /* maxInterStageShaderComponents */    60,
        /* maxComputeWorkgroupStorageSize */    16 * KB,
        /* maxComputeInvocationsPerWorkgroup */    512,
        /* maxComputeWorkgroupSizeX */    512,
        /* maxComputeWorkgroupSizeY */    512,
        /* maxComputeWorkgroupSizeZ */    512,
        /* maxComputeWorkgroupsPerDimension */    std::numeric_limits<uint32_t>::max(),
    };
}

constexpr static WGPULimits apple4()
{
    uint32_t maxBindGroups = 30;

    return WGPULimits {
        /* maxTextureDimension1D */    16384,
        /* maxTextureDimension2D */    16384,
        /* maxTextureDimension3D */    2048,
        /* maxTextureArrayLayers */    2048,
        /* maxBindGroups */    maxBindGroups,
        /* maxDynamicUniformBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxDynamicStorageBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxSampledTexturesPerShaderStage */    maxBindGroups * 96,
        /* maxSamplersPerShaderStage */    maxBindGroups * 16,
        /* maxStorageBuffersPerShaderStage */    maxBindGroups * 96,
        /* maxStorageTexturesPerShaderStage */    maxBindGroups * 96,
        /* maxUniformBuffersPerShaderStage */    maxBindGroups * 96,
        /* maxUniformBufferBindingSize */    0, // To be filled in by the caller.
        /* maxStorageBufferBindingSize */    0, // To be filled in by the caller.
        /* minUniformBufferOffsetAlignment */    4,
        /* minStorageBufferOffsetAlignment */    4,
        /* maxVertexBuffers */    30,
        /* maxVertexAttributes */    31,
        /* maxVertexBufferArrayStride */    std::numeric_limits<uint32_t>::max(),
        /* maxInterStageShaderComponents */    124,
        /* maxComputeWorkgroupStorageSize */    32 * KB,
        /* maxComputeInvocationsPerWorkgroup */    1024,
        /* maxComputeWorkgroupSizeX */    1024,
        /* maxComputeWorkgroupSizeY */    1024,
        /* maxComputeWorkgroupSizeZ */    1024,
        /* maxComputeWorkgroupsPerDimension */    std::numeric_limits<uint32_t>::max(),
    };
}

constexpr static WGPULimits apple5()
{
    uint32_t maxBindGroups = 30;

    return WGPULimits {
        /* maxTextureDimension1D */    16384,
        /* maxTextureDimension2D */    16384,
        /* maxTextureDimension3D */    2048,
        /* maxTextureArrayLayers */    2048,
        /* maxBindGroups */    maxBindGroups,
        /* maxDynamicUniformBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxDynamicStorageBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxSampledTexturesPerShaderStage */    maxBindGroups * 96,
        /* maxSamplersPerShaderStage */    maxBindGroups * 16,
        /* maxStorageBuffersPerShaderStage */    maxBindGroups * 96,
        /* maxStorageTexturesPerShaderStage */    maxBindGroups * 96,
        /* maxUniformBuffersPerShaderStage */    maxBindGroups * 96,
        /* maxUniformBufferBindingSize */    0, // To be filled in by the caller.
        /* maxStorageBufferBindingSize */    0, // To be filled in by the caller.
        /* minUniformBufferOffsetAlignment */    4,
        /* minStorageBufferOffsetAlignment */    4,
        /* maxVertexBuffers */    30,
        /* maxVertexAttributes */    31,
        /* maxVertexBufferArrayStride */    std::numeric_limits<uint32_t>::max(),
        /* maxInterStageShaderComponents */    124,
        /* maxComputeWorkgroupStorageSize */    32 * KB,
        /* maxComputeInvocationsPerWorkgroup */    1024,
        /* maxComputeWorkgroupSizeX */    1024,
        /* maxComputeWorkgroupSizeY */    1024,
        /* maxComputeWorkgroupSizeZ */    1024,
        /* maxComputeWorkgroupsPerDimension */    std::numeric_limits<uint32_t>::max(),
    };
}

#if !PLATFORM(WATCHOS)
constexpr static WGPULimits apple6()
{
    uint32_t maxBindGroups = 30;

    return WGPULimits {
        /* maxTextureDimension1D */    16384,
        /* maxTextureDimension2D */    16384,
        /* maxTextureDimension3D */    2048,
        /* maxTextureArrayLayers */    2048,
        /* maxBindGroups */    maxBindGroups,
        /* maxDynamicUniformBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxDynamicStorageBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxSampledTexturesPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxSamplersPerShaderStage */    maxBindGroups * 1024,
        /* maxStorageBuffersPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxStorageTexturesPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxUniformBuffersPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxUniformBufferBindingSize */    0, // To be filled in by the caller.
        /* maxStorageBufferBindingSize */    0, // To be filled in by the caller.
        /* minUniformBufferOffsetAlignment */    4,
        /* minStorageBufferOffsetAlignment */    4,
        /* maxVertexBuffers */    30,
        /* maxVertexAttributes */    31,
        /* maxVertexBufferArrayStride */    std::numeric_limits<uint32_t>::max(),
        /* maxInterStageShaderComponents */    124,
        /* maxComputeWorkgroupStorageSize */    32 * KB,
        /* maxComputeInvocationsPerWorkgroup */    1024,
        /* maxComputeWorkgroupSizeX */    1024,
        /* maxComputeWorkgroupSizeY */    1024,
        /* maxComputeWorkgroupSizeZ */    1024,
        /* maxComputeWorkgroupsPerDimension */    std::numeric_limits<uint32_t>::max(),
    };
}

constexpr static WGPULimits apple7()
{
    uint32_t maxBindGroups = 30;

    return WGPULimits {
        /* maxTextureDimension1D */    16384,
        /* maxTextureDimension2D */    16384,
        /* maxTextureDimension3D */    2048,
        /* maxTextureArrayLayers */    2048,
        /* maxBindGroups */    maxBindGroups,
        /* maxDynamicUniformBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxDynamicStorageBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxSampledTexturesPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxSamplersPerShaderStage */    maxBindGroups * 1024,
        /* maxStorageBuffersPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxStorageTexturesPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxUniformBuffersPerShaderStage */    maxBindGroups * 500000 / 2,
        /* maxUniformBufferBindingSize */    0, // To be filled in by the caller.
        /* maxStorageBufferBindingSize */    0, // To be filled in by the caller.
        /* minUniformBufferOffsetAlignment */    4,
        /* minStorageBufferOffsetAlignment */    4,
        /* maxVertexBuffers */    30,
        /* maxVertexAttributes */    31,
        /* maxVertexBufferArrayStride */    std::numeric_limits<uint32_t>::max(),
        /* maxInterStageShaderComponents */    124,
        /* maxComputeWorkgroupStorageSize */    32 * KB,
        /* maxComputeInvocationsPerWorkgroup */    1024,
        /* maxComputeWorkgroupSizeX */    1024,
        /* maxComputeWorkgroupSizeY */    1024,
        /* maxComputeWorkgroupSizeZ */    1024,
        /* maxComputeWorkgroupsPerDimension */    std::numeric_limits<uint32_t>::max(),
    };
}
#endif

static WGPULimits mac2(id<MTLDevice> device)
{
    uint32_t buffersPerBindGroup = 0;
    uint32_t texturesPerBindGroup = 0;
    uint32_t samplersPerBindGroup = 0;
    switch ([device argumentBuffersSupport]) {
    case MTLArgumentBuffersTier1:
        buffersPerBindGroup = 64;
        texturesPerBindGroup = 128;
        samplersPerBindGroup = 16;
        break;
    case MTLArgumentBuffersTier2:
        buffersPerBindGroup = 500000 / 2;
        texturesPerBindGroup = 500000 / 2;
        samplersPerBindGroup = 1024;
        break;
    }

    uint32_t maxBindGroups = 30;

    return WGPULimits {
        /* maxTextureDimension1D */    16384,
        /* maxTextureDimension2D */    16384,
        /* maxTextureDimension3D */    2048,
        /* maxTextureArrayLayers */    2048,
        /* maxBindGroups */    maxBindGroups,
        /* maxDynamicUniformBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxDynamicStorageBuffersPerPipelineLayout */    std::numeric_limits<uint32_t>::max(),
        /* maxSampledTexturesPerShaderStage */    maxBindGroups * texturesPerBindGroup,
        /* maxSamplersPerShaderStage */    maxBindGroups * samplersPerBindGroup,
        /* maxStorageBuffersPerShaderStage */    maxBindGroups * buffersPerBindGroup,
        /* maxStorageTexturesPerShaderStage */    maxBindGroups * texturesPerBindGroup,
        /* maxUniformBuffersPerShaderStage */    maxBindGroups * buffersPerBindGroup,
        /* maxUniformBufferBindingSize */    0, // To be filled in by the caller.
        /* maxStorageBufferBindingSize */    0, // To be filled in by the caller.
        /* minUniformBufferOffsetAlignment */    256,
        /* minStorageBufferOffsetAlignment */    256,
        /* maxVertexBuffers */    30,
        /* maxVertexAttributes */    31,
        /* maxVertexBufferArrayStride */    std::numeric_limits<uint32_t>::max(),
        /* maxInterStageShaderComponents */    32,
        /* maxComputeWorkgroupStorageSize */    32 * KB,
        /* maxComputeInvocationsPerWorkgroup */    1024,
        /* maxComputeWorkgroupSizeX */    1024,
        /* maxComputeWorkgroupSizeY */    1024,
        /* maxComputeWorkgroupSizeZ */    1024,
        /* maxComputeWorkgroupsPerDimension */    std::numeric_limits<uint32_t>::max(),
    };
}

static std::optional<WGPULimits> rawLimits(id<MTLDevice> device)
{
    std::optional<WGPULimits> result;

    auto merge = [&](const WGPULimits& limits) {
        if (!result) {
            result = limits;
            return;
        }

        // https://gpuweb.github.io/gpuweb/#limit-class-maximum
        auto mergeMaximum = [](auto previous, auto next) {
            return std::max(previous, next);
        };

        // https://gpuweb.github.io/gpuweb/#limit-class-alignment
        auto mergeAlignment = [](auto previous, auto next) {
            return std::min(WTF::roundUpToPowerOfTwo(previous), WTF::roundUpToPowerOfTwo(next));
        };

        result->maxTextureDimension1D = mergeMaximum(result->maxTextureDimension1D, limits.maxTextureDimension1D);
        result->maxTextureDimension2D = mergeMaximum(result->maxTextureDimension2D, limits.maxTextureDimension2D);
        result->maxTextureDimension3D = mergeMaximum(result->maxTextureDimension3D, limits.maxTextureDimension3D);
        result->maxTextureArrayLayers = mergeMaximum(result->maxTextureArrayLayers, limits.maxTextureArrayLayers);
        result->maxBindGroups = mergeMaximum(result->maxBindGroups, limits.maxBindGroups);
        result->maxDynamicUniformBuffersPerPipelineLayout = mergeMaximum(result->maxDynamicUniformBuffersPerPipelineLayout, limits.maxDynamicUniformBuffersPerPipelineLayout);
        result->maxDynamicStorageBuffersPerPipelineLayout = mergeMaximum(result->maxDynamicStorageBuffersPerPipelineLayout, limits.maxDynamicStorageBuffersPerPipelineLayout);
        result->maxSampledTexturesPerShaderStage = mergeMaximum(result->maxSampledTexturesPerShaderStage, limits.maxSampledTexturesPerShaderStage);
        result->maxSamplersPerShaderStage = mergeMaximum(result->maxSamplersPerShaderStage, limits.maxSamplersPerShaderStage);
        result->maxStorageBuffersPerShaderStage = mergeMaximum(result->maxStorageBuffersPerShaderStage, limits.maxStorageBuffersPerShaderStage);
        result->maxStorageTexturesPerShaderStage = mergeMaximum(result->maxStorageTexturesPerShaderStage, limits.maxStorageTexturesPerShaderStage);
        result->maxUniformBuffersPerShaderStage = mergeMaximum(result->maxUniformBuffersPerShaderStage, limits.maxUniformBuffersPerShaderStage);
        result->maxUniformBufferBindingSize = mergeMaximum(result->maxUniformBufferBindingSize, limits.maxUniformBufferBindingSize);
        result->maxStorageBufferBindingSize = mergeMaximum(result->maxStorageBufferBindingSize, limits.maxStorageBufferBindingSize);
        result->minUniformBufferOffsetAlignment = mergeAlignment(result->minUniformBufferOffsetAlignment, limits.minUniformBufferOffsetAlignment);
        result->minStorageBufferOffsetAlignment = mergeAlignment(result->minStorageBufferOffsetAlignment, limits.minStorageBufferOffsetAlignment);
        result->maxVertexBuffers = mergeMaximum(result->maxVertexBuffers, limits.maxVertexBuffers);
        result->maxVertexAttributes = mergeMaximum(result->maxVertexAttributes, limits.maxVertexAttributes);
        result->maxVertexBufferArrayStride = mergeMaximum(result->maxVertexBufferArrayStride, limits.maxVertexBufferArrayStride);
        result->maxInterStageShaderComponents = mergeMaximum(result->maxInterStageShaderComponents, limits.maxInterStageShaderComponents);
        result->maxComputeWorkgroupStorageSize = mergeMaximum(result->maxComputeWorkgroupStorageSize, limits.maxComputeWorkgroupStorageSize);
        result->maxComputeInvocationsPerWorkgroup = mergeMaximum(result->maxComputeInvocationsPerWorkgroup, limits.maxComputeInvocationsPerWorkgroup);
        result->maxComputeWorkgroupSizeX = mergeMaximum(result->maxComputeWorkgroupSizeX, limits.maxComputeWorkgroupSizeX);
        result->maxComputeWorkgroupSizeY = mergeMaximum(result->maxComputeWorkgroupSizeY, limits.maxComputeWorkgroupSizeY);
        result->maxComputeWorkgroupSizeZ = mergeMaximum(result->maxComputeWorkgroupSizeZ, limits.maxComputeWorkgroupSizeZ);
        result->maxComputeWorkgroupsPerDimension = mergeMaximum(result->maxComputeWorkgroupsPerDimension, limits.maxComputeWorkgroupsPerDimension);
    };

    // The feature set tables do not list limits for MTLGPUFamilyCommon1, MTLGPUFamilyCommon2, or MTLGPUFamilyCommon3.
    // MTLGPUFamilyApple1 and MTLGPUFamilyApple2 are not supported.
    if ([device supportsFamily:MTLGPUFamilyApple3])
        merge(apple3());
    if ([device supportsFamily:MTLGPUFamilyApple4])
        merge(apple4());
    if ([device supportsFamily:MTLGPUFamilyApple5])
        merge(apple5());
#if !PLATFORM(WATCHOS)
    if ([device supportsFamily:MTLGPUFamilyApple6])
        merge(apple6());
    if ([device supportsFamily:MTLGPUFamilyApple7])
        merge(apple7());
#endif
    // MTLGPUFamilyMac1 is not supported (yet?).
    if ([device supportsFamily:MTLGPUFamilyMac2])
        merge(mac2(device));

    if (result) {
        auto maxBufferLength = device.maxBufferLength;
        result->maxUniformBufferBindingSize = maxBufferLength;
        result->maxStorageBufferBindingSize = maxBufferLength;
    }

    return result;
}

constexpr static bool checkLimits(const WGPULimits& limits)
{
    // https://gpuweb.github.io/gpuweb/#limit-default
    auto defaultLimits = WGPULimits {
        /* maxTextureDimension1D */    8192,
        /* maxTextureDimension2D */    8192,
        /* maxTextureDimension3D */    2048,
        /* maxTextureArrayLayers */    256,
        /* maxBindGroups */    4,
        /* maxDynamicUniformBuffersPerPipelineLayout */    8,
        /* maxDynamicStorageBuffersPerPipelineLayout */    4,
        /* maxSampledTexturesPerShaderStage */    16,
        /* maxSamplersPerShaderStage */    16,
        /* maxStorageBuffersPerShaderStage */    8,
        /* maxStorageTexturesPerShaderStage */    4,
        /* maxUniformBuffersPerShaderStage */    12,
        /* maxUniformBufferBindingSize */    65536,
        /* maxStorageBufferBindingSize */    134217728,
        /* minUniformBufferOffsetAlignment */    256,
        /* minStorageBufferOffsetAlignment */    256,
        /* maxVertexBuffers */    8,
        /* maxVertexAttributes */    16,
        /* maxVertexBufferArrayStride */    2048,
        /* maxInterStageShaderComponents */    32,
        /* maxComputeWorkgroupStorageSize */    16352,
        /* maxComputeInvocationsPerWorkgroup */    256,
        /* maxComputeWorkgroupSizeX */    256,
        /* maxComputeWorkgroupSizeY */    256,
        /* maxComputeWorkgroupSizeZ */    64,
        /* maxComputeWorkgroupsPerDimension */    65535,
    };

    return !anyLimitIsBetterThan(defaultLimits, limits);
}

std::optional<WGPULimits> limits(id<MTLDevice> device)
{
    auto result = rawLimits(device);

    if (!result)
        return std::nullopt;

    if (!checkLimits(*result))
        return std::nullopt;

    return result;
}

bool isValid(const WGPULimits& limits)
{
    return isPowerOfTwo(limits.minUniformBufferOffsetAlignment) && isPowerOfTwo(limits.minStorageBufferOffsetAlignment);
}

bool anyLimitIsBetterThan(const WGPULimits& target, const WGPULimits& reference)
{
    if (target.maxTextureDimension1D > reference.maxTextureDimension1D)
        return true;
    if (target.maxTextureDimension2D > reference.maxTextureDimension2D)
        return true;
    if (target.maxTextureDimension3D > reference.maxTextureDimension3D)
        return true;
    if (target.maxTextureArrayLayers > reference.maxTextureArrayLayers)
        return true;
    if (target.maxBindGroups > reference.maxBindGroups)
        return true;
    if (target.maxDynamicUniformBuffersPerPipelineLayout > reference.maxDynamicUniformBuffersPerPipelineLayout)
        return true;
    if (target.maxDynamicStorageBuffersPerPipelineLayout > reference.maxDynamicStorageBuffersPerPipelineLayout)
        return true;
    if (target.maxSampledTexturesPerShaderStage > reference.maxSampledTexturesPerShaderStage)
        return true;
    if (target.maxSamplersPerShaderStage > reference.maxSamplersPerShaderStage)
        return true;
    if (target.maxStorageBuffersPerShaderStage > reference.maxStorageBuffersPerShaderStage)
        return true;
    if (target.maxStorageTexturesPerShaderStage > reference.maxStorageTexturesPerShaderStage)
        return true;
    if (target.maxUniformBuffersPerShaderStage > reference.maxUniformBuffersPerShaderStage)
        return true;
    if (target.maxUniformBufferBindingSize > reference.maxUniformBufferBindingSize)
        return true;
    if (target.maxStorageBufferBindingSize > reference.maxStorageBufferBindingSize)
        return true;
    if (target.minUniformBufferOffsetAlignment < reference.minUniformBufferOffsetAlignment)
        return true;
    if (target.minStorageBufferOffsetAlignment < reference.minStorageBufferOffsetAlignment)
        return true;
    if (target.maxVertexBuffers > reference.maxVertexBuffers)
        return true;
    if (target.maxVertexAttributes > reference.maxVertexAttributes)
        return true;
    if (target.maxVertexBufferArrayStride > reference.maxVertexBufferArrayStride)
        return true;
    if (target.maxInterStageShaderComponents > reference.maxInterStageShaderComponents)
        return true;
    if (target.maxComputeWorkgroupStorageSize > reference.maxComputeWorkgroupStorageSize)
        return true;
    if (target.maxComputeInvocationsPerWorkgroup > reference.maxComputeInvocationsPerWorkgroup)
        return true;
    if (target.maxComputeWorkgroupSizeX > reference.maxComputeWorkgroupSizeX)
        return true;
    if (target.maxComputeWorkgroupSizeY > reference.maxComputeWorkgroupSizeY)
        return true;
    if (target.maxComputeWorkgroupSizeZ > reference.maxComputeWorkgroupSizeZ)
        return true;
    if (target.maxComputeWorkgroupsPerDimension > reference.maxComputeWorkgroupsPerDimension)
        return true;

    return false;
}

} // namespace WebGPU
