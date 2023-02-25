/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#import "HardwareCapabilities.h"

#import <algorithm>
#import <limits>
#import <wtf/MathExtras.h>
#import <wtf/PageBlock.h>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

static constexpr uint32_t maxUInt32Limit()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=252873
    return std::numeric_limits<uint32_t>::max() - 1;
}

// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

// FIXME: https://github.com/gpuweb/gpuweb/issues/2749 we need more limits.

static HardwareCapabilities::BaseCapabilities baseCapabilities(id<MTLDevice> device)
{
    id<MTLCounterSet> timestampCounterSet = nil;
    id<MTLCounterSet> statisticCounterSet = nil;

    for (id<MTLCounterSet> counterSet in device.counterSets) {
        if ([counterSet.name isEqualToString:MTLCommonCounterSetTimestamp])
            timestampCounterSet = counterSet;
        else if ([counterSet.name isEqualToString:MTLCommonCounterSetStatistic])
            statisticCounterSet = counterSet;
    }

    auto counterSamplingAPI = HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary;
    if ([device supportsCounterSampling:MTLCounterSamplingPointAtBlitBoundary]
        && [device supportsCounterSampling:MTLCounterSamplingPointAtDispatchBoundary]
        && [device supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary])
        counterSamplingAPI = HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary;
    else if ([device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
        counterSamplingAPI = HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary;
    else
        timestampCounterSet = nil;

    return {
        [device argumentBuffersSupport],
        false, // To be filled in by the caller.
        timestampCounterSet,
        statisticCounterSet,
        false, // To be filled in by the caller.
        counterSamplingAPI,
    };
}

static Vector<WGPUFeatureName> baseFeatures(id<MTLDevice> device, const HardwareCapabilities::BaseCapabilities& baseCapabilities)
{
    Vector<WGPUFeatureName> features;

    features.append(WGPUFeatureName_DepthClipControl);
    features.append(WGPUFeatureName_Depth32FloatStencil8);

    if (baseCapabilities.timestampCounterSet)
        features.append(WGPUFeatureName_TimestampQuery);

    if (baseCapabilities.statisticCounterSet)
        features.append(WGPUFeatureName_PipelineStatisticsQuery);

#if PLATFORM(MAC)
    if (device.supportsBCTextureCompression)
        features.append(WGPUFeatureName_TextureCompressionBC);
#else
    UNUSED_PARAM(device);
#endif

    // WGPUFeatureName_TextureCompressionETC2 and WGPUFeatureName_TextureCompressionASTC are to be filled in by the caller.

    features.append(WGPUFeatureName_IndirectFirstInstance);
    features.append(WGPUFeatureName_RG11B10UfloatRenderable);
    features.append(WGPUFeatureName_ShaderF16);
    features.append(static_cast<WGPUFeatureName>(WGPUFeatureName_BGRA8UnormStorage));

    return features;
}

static HardwareCapabilities apple3(id<MTLDevice> device)
{
    auto baseCapabilities = WebGPU::baseCapabilities(device);

    baseCapabilities.supportsNonPrivateDepthStencilTextures = true;
    baseCapabilities.canPresentRGB10A2PixelFormats = false;

    auto features = WebGPU::baseFeatures(device, baseCapabilities);

    features.append(WGPUFeatureName_TextureCompressionETC2);
    features.append(WGPUFeatureName_TextureCompressionASTC);

    std::sort(features.begin(), features.end());

    uint32_t maxBindGroups = 30;

    return {
        {
            .maxTextureDimension1D =    16384,
            .maxTextureDimension2D =    16384,
            .maxTextureDimension3D =    2048,
            .maxTextureArrayLayers =    2048,
            .maxBindGroups =    maxBindGroups,
            .maxBindingsPerBindGroup =  640,
            .maxDynamicUniformBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxDynamicStorageBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxSampledTexturesPerShaderStage =    maxBindGroups * 31,
            .maxSamplersPerShaderStage =    maxBindGroups * 16,
            .maxStorageBuffersPerShaderStage =    maxBindGroups * 31,
            .maxStorageTexturesPerShaderStage =    maxBindGroups * 31,
            .maxUniformBuffersPerShaderStage =    maxBindGroups * 31,
            .maxUniformBufferBindingSize =    0, // To be filled in by the caller.
            .maxStorageBufferBindingSize =    0, // To be filled in by the caller.
            .minUniformBufferOffsetAlignment =    32,
            .minStorageBufferOffsetAlignment =    32,
            .maxVertexBuffers =    30,
            .maxBufferSize = device.maxBufferLength,
            .maxVertexAttributes =    31,
            .maxVertexBufferArrayStride =    maxUInt32Limit(),
            .maxInterStageShaderComponents =    60,
            .maxInterStageShaderVariables =    124,
            .maxColorAttachments =    8,
            .maxColorAttachmentBytesPerSample = 32,
            .maxComputeWorkgroupStorageSize =    16 * KB,
            .maxComputeInvocationsPerWorkgroup =    512,
            .maxComputeWorkgroupSizeX =    512,
            .maxComputeWorkgroupSizeY =    512,
            .maxComputeWorkgroupSizeZ =    512,
            .maxComputeWorkgroupsPerDimension =    maxUInt32Limit(),
        },
        WTFMove(features),
        baseCapabilities,
    };
}

static HardwareCapabilities apple4(id<MTLDevice> device)
{
    auto baseCapabilities = WebGPU::baseCapabilities(device);

    baseCapabilities.supportsNonPrivateDepthStencilTextures = true;
    baseCapabilities.canPresentRGB10A2PixelFormats = false;

    auto features = WebGPU::baseFeatures(device, baseCapabilities);

    features.append(WGPUFeatureName_TextureCompressionETC2);
    features.append(WGPUFeatureName_TextureCompressionASTC);

    std::sort(features.begin(), features.end());

    uint32_t maxBindGroups = 30;

    return {
        {
            .maxTextureDimension1D =    16384,
            .maxTextureDimension2D =    16384,
            .maxTextureDimension3D =    2048,
            .maxTextureArrayLayers =    2048,
            .maxBindGroups =    maxBindGroups,
            .maxBindingsPerBindGroup =  640,
            .maxDynamicUniformBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxDynamicStorageBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxSampledTexturesPerShaderStage =    maxBindGroups * 96,
            .maxSamplersPerShaderStage =    maxBindGroups * 16,
            .maxStorageBuffersPerShaderStage =    maxBindGroups * 96,
            .maxStorageTexturesPerShaderStage =    maxBindGroups * 96,
            .maxUniformBuffersPerShaderStage =    maxBindGroups * 96,
            .maxUniformBufferBindingSize =    0, // To be filled in by the caller.
            .maxStorageBufferBindingSize =    0, // To be filled in by the caller.
            .minUniformBufferOffsetAlignment =    32,
            .minStorageBufferOffsetAlignment =    32,
            .maxVertexBuffers =    30,
            .maxBufferSize =    device.maxBufferLength,
            .maxVertexAttributes =    31,
            .maxVertexBufferArrayStride =    maxUInt32Limit(),
            .maxInterStageShaderComponents =    124,
            .maxInterStageShaderVariables =    124,
            .maxColorAttachments =    8,
            .maxColorAttachmentBytesPerSample = 32,
            .maxComputeWorkgroupStorageSize =    32 * KB,
            .maxComputeInvocationsPerWorkgroup =    1024,
            .maxComputeWorkgroupSizeX =    1024,
            .maxComputeWorkgroupSizeY =    1024,
            .maxComputeWorkgroupSizeZ =    1024,
            .maxComputeWorkgroupsPerDimension =    maxUInt32Limit(),
        },
        WTFMove(features),
        baseCapabilities,
    };
}

static HardwareCapabilities apple5(id<MTLDevice> device)
{
    auto baseCapabilities = WebGPU::baseCapabilities(device);

    baseCapabilities.supportsNonPrivateDepthStencilTextures = true;
    baseCapabilities.canPresentRGB10A2PixelFormats = false;

    auto features = WebGPU::baseFeatures(device, baseCapabilities);

    features.append(WGPUFeatureName_TextureCompressionETC2);
    features.append(WGPUFeatureName_TextureCompressionASTC);

    std::sort(features.begin(), features.end());

    uint32_t maxBindGroups = 30;

    return {
        {
            .maxTextureDimension1D =    16384,
            .maxTextureDimension2D =    16384,
            .maxTextureDimension3D =    2048,
            .maxTextureArrayLayers =    2048,
            .maxBindGroups =    maxBindGroups,
            .maxBindingsPerBindGroup =  640,
            .maxDynamicUniformBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxDynamicStorageBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxSampledTexturesPerShaderStage =    maxBindGroups * 96,
            .maxSamplersPerShaderStage =    maxBindGroups * 16,
            .maxStorageBuffersPerShaderStage =    maxBindGroups * 96,
            .maxStorageTexturesPerShaderStage =    maxBindGroups * 96,
            .maxUniformBuffersPerShaderStage =    maxBindGroups * 96,
            .maxUniformBufferBindingSize =    0, // To be filled in by the caller.
            .maxStorageBufferBindingSize =    0, // To be filled in by the caller.
            .minUniformBufferOffsetAlignment =    32,
            .minStorageBufferOffsetAlignment =    32,
            .maxVertexBuffers =    30,
            .maxBufferSize =    device.maxBufferLength,
            .maxVertexAttributes =    31,
            .maxVertexBufferArrayStride =    maxUInt32Limit(),
            .maxInterStageShaderComponents =    124,
            .maxInterStageShaderVariables = 124,
            .maxColorAttachments = 8,
            .maxColorAttachmentBytesPerSample = 32,
            .maxComputeWorkgroupStorageSize =    32 * KB,
            .maxComputeInvocationsPerWorkgroup =    1024,
            .maxComputeWorkgroupSizeX =    1024,
            .maxComputeWorkgroupSizeY =    1024,
            .maxComputeWorkgroupSizeZ =    1024,
            .maxComputeWorkgroupsPerDimension =    maxUInt32Limit(),
        },
        WTFMove(features),
        baseCapabilities,
    };
}

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
static HardwareCapabilities apple6(id<MTLDevice> device)
{
    auto baseCapabilities = WebGPU::baseCapabilities(device);

    baseCapabilities.supportsNonPrivateDepthStencilTextures = true;
    baseCapabilities.canPresentRGB10A2PixelFormats = false;

    auto features = WebGPU::baseFeatures(device, baseCapabilities);

    features.append(WGPUFeatureName_TextureCompressionETC2);
    features.append(WGPUFeatureName_TextureCompressionASTC);

    std::sort(features.begin(), features.end());

    uint32_t maxBindGroups = 30;

    return {
        {
            .maxTextureDimension1D =    16384,
            .maxTextureDimension2D =    16384,
            .maxTextureDimension3D =    2048,
            .maxTextureArrayLayers =    2048,
            .maxBindGroups =    maxBindGroups,
            .maxBindingsPerBindGroup =    500000,
            .maxDynamicUniformBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxDynamicStorageBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxSampledTexturesPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxSamplersPerShaderStage =    maxBindGroups * 1024,
            .maxStorageBuffersPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxStorageTexturesPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxUniformBuffersPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxUniformBufferBindingSize =    0, // To be filled in by the caller.
            .maxStorageBufferBindingSize =    0, // To be filled in by the caller.
            .minUniformBufferOffsetAlignment =    32,
            .minStorageBufferOffsetAlignment =    32,
            .maxVertexBuffers =    30,
            .maxBufferSize = device.maxBufferLength,
            .maxVertexAttributes =    31,
            .maxVertexBufferArrayStride =    maxUInt32Limit(),
            .maxInterStageShaderComponents =    124,
            .maxInterStageShaderVariables = 124,
            .maxColorAttachments = 8,
            .maxColorAttachmentBytesPerSample = 32,
            .maxComputeWorkgroupStorageSize =    32 * KB,
            .maxComputeInvocationsPerWorkgroup =    1024,
            .maxComputeWorkgroupSizeX =    1024,
            .maxComputeWorkgroupSizeY =    1024,
            .maxComputeWorkgroupSizeZ =    1024,
            .maxComputeWorkgroupsPerDimension =    maxUInt32Limit(),
        },
        WTFMove(features),
        baseCapabilities,
    };
}

static HardwareCapabilities apple7(id<MTLDevice> device)
{
    auto baseCapabilities = WebGPU::baseCapabilities(device);

    baseCapabilities.supportsNonPrivateDepthStencilTextures = true;
    baseCapabilities.canPresentRGB10A2PixelFormats = false;

    auto features = WebGPU::baseFeatures(device, baseCapabilities);

    features.append(WGPUFeatureName_TextureCompressionETC2);
    features.append(WGPUFeatureName_TextureCompressionASTC);

    std::sort(features.begin(), features.end());

    uint32_t maxBindGroups = 30;

    return {
        {
            .maxTextureDimension1D =    16384,
            .maxTextureDimension2D =    16384,
            .maxTextureDimension3D =    2048,
            .maxTextureArrayLayers =    2048,
            .maxBindGroups =    maxBindGroups,
            .maxBindingsPerBindGroup =    500000,
            .maxDynamicUniformBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxDynamicStorageBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxSampledTexturesPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxSamplersPerShaderStage =    maxBindGroups * 1024,
            .maxStorageBuffersPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxStorageTexturesPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxUniformBuffersPerShaderStage =    maxBindGroups * 500000 / 2,
            .maxUniformBufferBindingSize =    0, // To be filled in by the caller.
            .maxStorageBufferBindingSize =    0, // To be filled in by the caller.
            .minUniformBufferOffsetAlignment =    32,
            .minStorageBufferOffsetAlignment =    32,
            .maxVertexBuffers =    30,
            .maxBufferSize = device.maxBufferLength,
            .maxVertexAttributes =    31,
            .maxVertexBufferArrayStride =    maxUInt32Limit(),
            .maxInterStageShaderComponents =    124,
            .maxInterStageShaderVariables =    124,
            .maxColorAttachments = 8,
            .maxColorAttachmentBytesPerSample = 32,
            .maxComputeWorkgroupStorageSize =    32 * KB,
            .maxComputeInvocationsPerWorkgroup =    1024,
            .maxComputeWorkgroupSizeX =    1024,
            .maxComputeWorkgroupSizeY =    1024,
            .maxComputeWorkgroupSizeZ =    1024,
            .maxComputeWorkgroupsPerDimension =    maxUInt32Limit(),
        },
        WTFMove(features),
        baseCapabilities,
    };
}
#endif

static HardwareCapabilities mac2(id<MTLDevice> device)
{
    auto baseCapabilities = WebGPU::baseCapabilities(device);

    baseCapabilities.supportsNonPrivateDepthStencilTextures = false;
    baseCapabilities.canPresentRGB10A2PixelFormats = true;

    auto features = WebGPU::baseFeatures(device, baseCapabilities);

    std::sort(features.begin(), features.end());

    uint32_t buffersPerBindGroup = 0;
    uint32_t texturesPerBindGroup = 0;
    uint32_t samplersPerBindGroup = 0;
    switch (baseCapabilities.argumentBuffersTier) {
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

    return {
        {
            .maxTextureDimension1D =    16384,
            .maxTextureDimension2D =    16384,
            .maxTextureDimension3D =    2048,
            .maxTextureArrayLayers =    2048,
            .maxBindGroups =    maxBindGroups,
            .maxBindingsPerBindGroup =  640,
            .maxDynamicUniformBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxDynamicStorageBuffersPerPipelineLayout =    maxUInt32Limit(),
            .maxSampledTexturesPerShaderStage =    maxBindGroups * texturesPerBindGroup,
            .maxSamplersPerShaderStage =    maxBindGroups * samplersPerBindGroup,
            .maxStorageBuffersPerShaderStage =    maxBindGroups * buffersPerBindGroup,
            .maxStorageTexturesPerShaderStage =    maxBindGroups * texturesPerBindGroup,
            .maxUniformBuffersPerShaderStage =    maxBindGroups * buffersPerBindGroup,
            .maxUniformBufferBindingSize =    0, // To be filled in by the caller.
            .maxStorageBufferBindingSize =    0, // To be filled in by the caller.
            .minUniformBufferOffsetAlignment =    256,
            .minStorageBufferOffsetAlignment =    256,
            .maxVertexBuffers =    30,
            .maxBufferSize =    device.maxBufferLength,
            .maxVertexAttributes =    31,
            .maxVertexBufferArrayStride =    maxUInt32Limit(),
            .maxInterStageShaderComponents =    60,
            .maxInterStageShaderVariables =    32,
            .maxColorAttachments =    8,
            .maxColorAttachmentBytesPerSample = 32,
            .maxComputeWorkgroupStorageSize =    32 * KB,
            .maxComputeInvocationsPerWorkgroup =    1024,
            .maxComputeWorkgroupSizeX =    1024,
            .maxComputeWorkgroupSizeY =    1024,
            .maxComputeWorkgroupSizeZ =    1024,
            .maxComputeWorkgroupsPerDimension =    maxUInt32Limit(),
        },
        WTFMove(features),
        baseCapabilities,
    };
}

template <typename T>
static T mergeMaximum(T previous, T next)
{
    // https://gpuweb.github.io/gpuweb/#limit-class-maximum
    return std::max(previous, next);
};

template <typename T>
static T mergeAlignment(T previous, T next)
{
    // https://gpuweb.github.io/gpuweb/#limit-class-alignment
    return std::min(WTF::roundUpToPowerOfTwo(previous), WTF::roundUpToPowerOfTwo(next));
};

static WGPULimits mergeLimits(const WGPULimits& previous, const WGPULimits& next)
{
    return {
        .maxTextureDimension1D = mergeMaximum(previous.maxTextureDimension1D, next.maxTextureDimension1D),
        .maxTextureDimension2D = mergeMaximum(previous.maxTextureDimension2D, next.maxTextureDimension2D),
        .maxTextureDimension3D = mergeMaximum(previous.maxTextureDimension3D, next.maxTextureDimension3D),
        .maxTextureArrayLayers = mergeMaximum(previous.maxTextureArrayLayers, next.maxTextureArrayLayers),
        .maxBindGroups = mergeMaximum(previous.maxBindGroups, next.maxBindGroups),
        .maxBindingsPerBindGroup = mergeMaximum(previous.maxBindingsPerBindGroup, next.maxBindingsPerBindGroup),
        .maxDynamicUniformBuffersPerPipelineLayout = mergeMaximum(previous.maxDynamicUniformBuffersPerPipelineLayout, next.maxDynamicUniformBuffersPerPipelineLayout),
        .maxDynamicStorageBuffersPerPipelineLayout = mergeMaximum(previous.maxDynamicStorageBuffersPerPipelineLayout, next.maxDynamicStorageBuffersPerPipelineLayout),
        .maxSampledTexturesPerShaderStage = mergeMaximum(previous.maxSampledTexturesPerShaderStage, next.maxSampledTexturesPerShaderStage),
        .maxSamplersPerShaderStage = mergeMaximum(previous.maxSamplersPerShaderStage, next.maxSamplersPerShaderStage),
        .maxStorageBuffersPerShaderStage = mergeMaximum(previous.maxStorageBuffersPerShaderStage, next.maxStorageBuffersPerShaderStage),
        .maxStorageTexturesPerShaderStage = mergeMaximum(previous.maxStorageTexturesPerShaderStage, next.maxStorageTexturesPerShaderStage),
        .maxUniformBuffersPerShaderStage = mergeMaximum(previous.maxUniformBuffersPerShaderStage, next.maxUniformBuffersPerShaderStage),
        .maxUniformBufferBindingSize = mergeMaximum(previous.maxUniformBufferBindingSize, next.maxUniformBufferBindingSize),
        .maxStorageBufferBindingSize = mergeMaximum(previous.maxStorageBufferBindingSize, next.maxStorageBufferBindingSize),
        .minUniformBufferOffsetAlignment = mergeAlignment(previous.minUniformBufferOffsetAlignment, next.minUniformBufferOffsetAlignment),
        .minStorageBufferOffsetAlignment = mergeAlignment(previous.minStorageBufferOffsetAlignment, next.minStorageBufferOffsetAlignment),
        .maxVertexBuffers = mergeMaximum(previous.maxVertexBuffers, next.maxVertexBuffers),
        .maxBufferSize = mergeMaximum(previous.maxBufferSize, next.maxBufferSize),
        .maxVertexAttributes = mergeMaximum(previous.maxVertexAttributes, next.maxVertexAttributes),
        .maxVertexBufferArrayStride = mergeMaximum(previous.maxVertexBufferArrayStride, next.maxVertexBufferArrayStride),
        .maxInterStageShaderComponents = mergeMaximum(previous.maxInterStageShaderComponents, next.maxInterStageShaderComponents),
        .maxInterStageShaderVariables = mergeMaximum(previous.maxInterStageShaderVariables, next.maxInterStageShaderVariables),
        .maxColorAttachments = mergeMaximum(previous.maxColorAttachments, next.maxColorAttachments),
        .maxColorAttachmentBytesPerSample = mergeMaximum(previous.maxColorAttachmentBytesPerSample, next.maxColorAttachmentBytesPerSample),
        .maxComputeWorkgroupStorageSize = mergeMaximum(previous.maxComputeWorkgroupStorageSize, next.maxComputeWorkgroupStorageSize),
        .maxComputeInvocationsPerWorkgroup = mergeMaximum(previous.maxComputeInvocationsPerWorkgroup, next.maxComputeInvocationsPerWorkgroup),
        .maxComputeWorkgroupSizeX = mergeMaximum(previous.maxComputeWorkgroupSizeX, next.maxComputeWorkgroupSizeX),
        .maxComputeWorkgroupSizeY = mergeMaximum(previous.maxComputeWorkgroupSizeY, next.maxComputeWorkgroupSizeY),
        .maxComputeWorkgroupSizeZ = mergeMaximum(previous.maxComputeWorkgroupSizeZ, next.maxComputeWorkgroupSizeZ),
        .maxComputeWorkgroupsPerDimension = mergeMaximum(previous.maxComputeWorkgroupsPerDimension, next.maxComputeWorkgroupsPerDimension),
    };
};

static Vector<WGPUFeatureName> mergeFeatures(const Vector<WGPUFeatureName>& previous, const Vector<WGPUFeatureName>& next)
{
    ASSERT(WTF::isSortedConstExpr(previous.begin(), previous.end()));
    ASSERT(WTF::isSortedConstExpr(next.begin(), next.end()));

    Vector<WGPUFeatureName> result(previous.size() + next.size());
    auto end = mergeDeduplicatedSorted(previous.begin(), previous.end(), next.begin(), next.end(), result.begin());
    result.resize(end - result.begin());
    return result;
}

static HardwareCapabilities::BaseCapabilities mergeBaseCapabilities(const HardwareCapabilities::BaseCapabilities& previous, const HardwareCapabilities::BaseCapabilities& next)
{
    ASSERT(previous.argumentBuffersTier == next.argumentBuffersTier);
    ASSERT(!previous.timestampCounterSet || [previous.timestampCounterSet isEqual:next.timestampCounterSet]);
    ASSERT(!previous.statisticCounterSet || [previous.statisticCounterSet isEqual:next.statisticCounterSet]);
    ASSERT(previous.counterSamplingAPI == next.counterSamplingAPI);
    return {
        previous.argumentBuffersTier,
        previous.supportsNonPrivateDepthStencilTextures || next.supportsNonPrivateDepthStencilTextures,
        previous.timestampCounterSet,
        previous.statisticCounterSet,
        previous.canPresentRGB10A2PixelFormats || next.canPresentRGB10A2PixelFormats,
        previous.counterSamplingAPI,
    };
}

static std::optional<HardwareCapabilities> rawHardwareCapabilities(id<MTLDevice> device)
{
    std::optional<HardwareCapabilities> result;

    auto merge = [&](const HardwareCapabilities& capabilities) {
        if (!result) {
            result = capabilities;
            return;
        }

        result->limits = mergeLimits(result->limits, capabilities.limits);
        result->features = mergeFeatures(result->features, capabilities.features);
        result->baseCapabilities = mergeBaseCapabilities(result->baseCapabilities, capabilities.baseCapabilities);
    };

    // The feature set tables do not list limits for MTLGPUFamilyCommon1, MTLGPUFamilyCommon2, or MTLGPUFamilyCommon3.
    // MTLGPUFamilyApple1 and MTLGPUFamilyApple2 are not supported.
    if ([device supportsFamily:MTLGPUFamilyApple3])
        merge(apple3(device));
    if ([device supportsFamily:MTLGPUFamilyApple4])
        merge(apple4(device));
    if ([device supportsFamily:MTLGPUFamilyApple5])
        merge(apple5(device));
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if ([device supportsFamily:MTLGPUFamilyApple6])
        merge(apple6(device));
    if ([device supportsFamily:MTLGPUFamilyApple7])
        merge(apple7(device));
#endif
    // MTLGPUFamilyMac1 is not supported (yet?).
    if ([device supportsFamily:MTLGPUFamilyMac2])
        merge(mac2(device));

    if (result) {
        auto maxBufferLength = device.maxBufferLength;
        result->limits.maxUniformBufferBindingSize = maxBufferLength;
        result->limits.maxStorageBufferBindingSize = maxBufferLength;
    }

    return result;
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
    if (target.maxBindingsPerBindGroup > reference.maxBindingsPerBindGroup)
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
    if (target.maxBufferSize > reference.maxBufferSize)
        return true;
    if (target.maxVertexAttributes > reference.maxVertexAttributes)
        return true;
    if (target.maxVertexBufferArrayStride > reference.maxVertexBufferArrayStride)
        return true;
    if (target.maxInterStageShaderComponents > reference.maxInterStageShaderComponents)
        return true;
    if (target.maxInterStageShaderVariables > reference.maxInterStageShaderVariables)
        return true;
    if (target.maxColorAttachments > reference.maxColorAttachments)
        return true;
    if (target.maxColorAttachmentBytesPerSample > reference.maxColorAttachmentBytesPerSample)
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

bool includesUnsupportedFeatures(const Vector<WGPUFeatureName>& target, const Vector<WGPUFeatureName>& reference)
{
    ASSERT(WTF::isSortedConstExpr(reference.begin(), reference.end()));
    for (auto feature : target) {
        if (!std::binary_search(reference.begin(), reference.end(), feature))
            return true;
    }
    return false;
}

WGPULimits defaultLimits()
{
    // https://gpuweb.github.io/gpuweb/#limit-default

    return {
        .maxTextureDimension1D =    8192,
        .maxTextureDimension2D =    8192,
        .maxTextureDimension3D =    2048,
        .maxTextureArrayLayers =    256,
        .maxBindGroups =    4,
        .maxBindingsPerBindGroup = 512,
        .maxDynamicUniformBuffersPerPipelineLayout =    8,
        .maxDynamicStorageBuffersPerPipelineLayout =    4,
        .maxSampledTexturesPerShaderStage =    16,
        .maxSamplersPerShaderStage =    16,
        .maxStorageBuffersPerShaderStage =    8,
        .maxStorageTexturesPerShaderStage =    4,
        .maxUniformBuffersPerShaderStage =    12,
        .maxUniformBufferBindingSize =    65536,
        .maxStorageBufferBindingSize =    134217728,
        .minUniformBufferOffsetAlignment =    256,
        .minStorageBufferOffsetAlignment =    256,
        .maxVertexBuffers =    8,
        .maxBufferSize = 134217728,
        .maxVertexAttributes =    16,
        .maxVertexBufferArrayStride =    2048,
        .maxInterStageShaderComponents =    60,
        .maxInterStageShaderVariables = 32,
        .maxColorAttachments = 8,
        .maxColorAttachmentBytesPerSample = 32,
        .maxComputeWorkgroupStorageSize =    16352,
        .maxComputeInvocationsPerWorkgroup =    256,
        .maxComputeWorkgroupSizeX =    256,
        .maxComputeWorkgroupSizeY =    256,
        .maxComputeWorkgroupSizeZ =    64,
        .maxComputeWorkgroupsPerDimension =    65535,
    };
}

std::optional<HardwareCapabilities> hardwareCapabilities(id<MTLDevice> device)
{
    auto result = rawHardwareCapabilities(device);

    if (!result)
        return std::nullopt;

    if (anyLimitIsBetterThan(defaultLimits(), result->limits))
        return std::nullopt;

    return result;
}

bool isValid(const WGPULimits& limits)
{
    return isPowerOfTwo(limits.minUniformBufferOffsetAlignment) && isPowerOfTwo(limits.minStorageBufferOffsetAlignment);
}

} // namespace WebGPU
