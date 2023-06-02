/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
#import "BindGroup.h"

#import "APIConversions.h"
#import "BindGroupLayout.h"
#import "Buffer.h"
#import "Device.h"
#import "Sampler.h"
#import "TextureView.h"
#import <wtf/EnumeratedArray.h>

namespace WebGPU {

static bool bufferIsPresent(const WGPUBindGroupEntry& entry)
{
    return entry.buffer;
}

static bool samplerIsPresent(const WGPUBindGroupEntry& entry)
{
    return entry.sampler;
}

static bool textureViewIsPresent(const WGPUBindGroupEntry& entry)
{
    return entry.textureView;
}

static MTLRenderStages metalRenderStage(ShaderStage shaderStage)
{
    switch (shaderStage) {
    case ShaderStage::Vertex:
        return MTLRenderStageVertex;
    case ShaderStage::Fragment:
        return MTLRenderStageFragment;
    case ShaderStage::Compute:
        return (MTLRenderStages)0;
    }
}

template <typename T>
using ShaderStageArray = EnumeratedArray<ShaderStage, T, ShaderStage::Compute>;

#if HAVE(COREVIDEO_METAL_SUPPORT)
static simd::float4x3 colorSpaceConversionMatrixForPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    auto format = CVPixelBufferGetPixelFormatType(pixelBuffer);
    UNUSED_PARAM(format);

    // FIXME: Implement other formats after https://bugs.webkit.org/show_bug.cgi?id=256724 is implemented
    return simd::float4x3(simd::make_float3(+1.16895f, +1.16895f, +1.16895f),
        simd::make_float3(-0.00012f, -0.21399f, +2.12073f),
        simd::make_float3(+1.79968f, -0.53503f, +0.00012f),
        simd::make_float3(-0.97284f, 0.30145f, -1.13348f));
}
#endif

Device::ExternalTextureData Device::createExternalTextureFromPixelBuffer(CVPixelBufferRef pixelBuffer, WGPUColorSpace colorSpace) const
{
#if HAVE(COREVIDEO_METAL_SUPPORT)
    UNUSED_PARAM(colorSpace);

    id<MTLTexture> mtlTexture0 = nil;
    id<MTLTexture> mtlTexture1 = nil;

    CVMetalTextureRef plane0 = nullptr;
    CVMetalTextureRef plane1 = nullptr;

    CVReturn status1 = CVMetalTextureCacheCreateTextureFromImage(nullptr, m_coreVideoTextureCache.get(), pixelBuffer, nullptr, MTLPixelFormatR8Unorm, CVPixelBufferGetWidthOfPlane(pixelBuffer, 0), CVPixelBufferGetHeightOfPlane(pixelBuffer, 0), 0, &plane0);
    CVReturn status2 = CVMetalTextureCacheCreateTextureFromImage(nullptr, m_coreVideoTextureCache.get(), pixelBuffer, nullptr, MTLPixelFormatRG8Unorm, CVPixelBufferGetWidthOfPlane(pixelBuffer, 1), CVPixelBufferGetHeightOfPlane(pixelBuffer, 1), 1, &plane1);

    float lowerLeft[2];
    float lowerRight[2];
    float upperRight[2];
    float upperLeft[2];

    if (status1 == kCVReturnSuccess) {
        mtlTexture0 = CVMetalTextureGetTexture(plane0);
        CVMetalTextureGetCleanTexCoords(plane0, lowerLeft, lowerRight, upperRight, upperLeft);
    } else {
        if (plane1)
            CFRelease(plane1);
        return { };
    }

    if (status2 == kCVReturnSuccess)
        mtlTexture1 = CVMetalTextureGetTexture(plane1);

    m_defaultQueue->onSubmittedWorkDone([plane0, plane1](WGPUQueueWorkDoneStatus) {
        if (plane0)
            CFRelease(plane0);

        if (plane1)
            CFRelease(plane1);
    });

    float Ax = 1.f / (upperRight[0] - lowerLeft[0]);
    float Bx = -Ax * lowerLeft[0];
    float Ay = 1.f / (lowerRight[1] - upperLeft[1]);
    float By = -Ay * upperLeft[1];
    simd::float3x2 uvRemappingMatrix = simd::float3x2(simd::make_float2(Ax, 0.f), simd::make_float2(0.f, Ay), simd::make_float2(Bx, By));
    simd::float4x3 colorSpaceConversionMatrix = colorSpaceConversionMatrixForPixelBuffer(pixelBuffer);

    return { mtlTexture0, mtlTexture1, uvRemappingMatrix, colorSpaceConversionMatrix };
#else
    UNUSED_PARAM(pixelBuffer);
    UNUSED_PARAM(colorSpace);
    return { };
#endif
}

Ref<BindGroup> Device::createBindGroup(const WGPUBindGroupDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return BindGroup::createInvalid(*this);

    // FIXME: We have to validate that the bind group is compatible with the bind group layout.
    // Otherwise, we are in crazytown.

    constexpr ShaderStage stages[] = { ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute };
    constexpr size_t stageCount = std::size(stages);
    ShaderStageArray<NSUInteger> bindingIndexForStage = std::array<NSUInteger, stageCount>();
    const auto& bindGroupLayout = WebGPU::fromAPI(descriptor.layout);
    if (!bindGroupLayout.isValid()) {
        generateAValidationError("invalid BindGroupLayout createBindGroup"_s);
        return BindGroup::createInvalid(*this);
    }

    Vector<BindableResource> resources;
    ShaderStageArray<id<MTLArgumentEncoder>> argumentEncoder = std::array<id<MTLArgumentEncoder>, stageCount>({ bindGroupLayout.vertexArgumentEncoder(), bindGroupLayout.fragmentArgumentEncoder(), bindGroupLayout.computeArgumentEncoder() });
    ShaderStageArray<id<MTLBuffer>> argumentBuffer;
    for (ShaderStage stage : stages) {
        auto encodedLength = bindGroupLayout.encodedLength(stage);
        argumentBuffer[stage] = encodedLength ? safeCreateBuffer(encodedLength, MTLStorageModeShared) : nil;
        [argumentEncoder[stage] setArgumentBuffer:argumentBuffer[stage] offset:0];
    }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=257190 The bind group layout determines the layout of what gets put into the bind group.
    // We should probably iterate over the bind group layout here, rather than the bind group.
    // For each entry in the bind group layout, we should find the corresponding member of the bind group, and then process it.
    for (uint32_t i = 0, entryCount = descriptor.entryCount; i < entryCount; ++i) {
        const WGPUBindGroupEntry& entry = descriptor.entries[i];

        WGPUExternalTexture wgpuExternalTexture = nullptr;
        if (entry.nextInChain) {
            if (entry.nextInChain->sType != static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupEntryExternalTexture)) {
                generateAValidationError("Unknown chain object in WGPUBindGroupEntry"_s);
                return BindGroup::createInvalid(*this);
            }
            if (entry.nextInChain->next) {
                generateAValidationError("Unknown chain object in WGPUBindGroupEntry"_s);
                return BindGroup::createInvalid(*this);
            }

            wgpuExternalTexture = reinterpret_cast<const WGPUBindGroupExternalTextureEntry*>(entry.nextInChain)->externalTexture;
        }

        bool bufferIsPresent = WebGPU::bufferIsPresent(entry);
        bool samplerIsPresent = WebGPU::samplerIsPresent(entry);
        bool textureViewIsPresent = WebGPU::textureViewIsPresent(entry);
        bool externalTextureIsPresent = static_cast<bool>(wgpuExternalTexture);
        if (bufferIsPresent + samplerIsPresent + textureViewIsPresent + externalTextureIsPresent != 1)
            return BindGroup::createInvalid(*this);

        for (ShaderStage stage : stages) {
            if (!bindGroupLayout.bindingContainsStage(entry.binding, stage))
                continue;

            auto& index = bindingIndexForStage[stage];
            if (bufferIsPresent) {
                id<MTLBuffer> buffer = WebGPU::fromAPI(entry.buffer).buffer();
                if (entry.offset > buffer.length)
                    return BindGroup::createInvalid(*this);

                [argumentEncoder[stage] setBuffer:buffer offset:entry.offset atIndex:index++];
                resources.append({ buffer, MTLResourceUsageRead, metalRenderStage(stage) });
            } else if (samplerIsPresent) {
                id<MTLSamplerState> sampler = WebGPU::fromAPI(entry.sampler).samplerState();
                [argumentEncoder[stage] setSamplerState:sampler atIndex:index++];
            } else if (textureViewIsPresent) {
                id<MTLTexture> texture = WebGPU::fromAPI(entry.textureView).texture();
                [argumentEncoder[stage] setTexture:texture atIndex:index++];
                resources.append({ texture, MTLResourceUsageRead, metalRenderStage(stage) });
            } else if (externalTextureIsPresent) {
                auto& externalTexture = WebGPU::fromAPI(wgpuExternalTexture);
                auto textureData = createExternalTextureFromPixelBuffer(externalTexture.pixelBuffer(), externalTexture.colorSpace());
                [argumentEncoder[stage] setTexture:textureData.texture0 atIndex:index++];
                [argumentEncoder[stage] setTexture:textureData.texture1 atIndex:index++];
                ASSERT(textureData.texture0);
                ASSERT(textureData.texture1);
                if (textureData.texture0)
                    resources.append({ textureData.texture0, MTLResourceUsageRead, metalRenderStage(stage) });
                if (textureData.texture1)
                    resources.append({ textureData.texture1, MTLResourceUsageRead, metalRenderStage(stage) });

                auto* uvRemapAddress = static_cast<simd::float3x2*>([argumentEncoder[stage] constantDataAtIndex:index++]);
                *uvRemapAddress = textureData.uvRemappingMatrix;

                auto* cscMatrixAddress = static_cast<simd::float4x3*>([argumentEncoder[stage] constantDataAtIndex:index++]);
                *cscMatrixAddress = textureData.colorSpaceConversionMatrix;
            }
        }
    }

    return BindGroup::create(argumentBuffer[ShaderStage::Vertex], argumentBuffer[ShaderStage::Fragment], argumentBuffer[ShaderStage::Compute], WTFMove(resources), *this);
}

BindGroup::BindGroup(id<MTLBuffer> vertexArgumentBuffer, id<MTLBuffer> fragmentArgumentBuffer, id<MTLBuffer> computeArgumentBuffer, Vector<BindableResource>&& resources, Device& device)
    : m_vertexArgumentBuffer(vertexArgumentBuffer)
    , m_fragmentArgumentBuffer(fragmentArgumentBuffer)
    , m_computeArgumentBuffer(computeArgumentBuffer)
    , m_device(device)
    , m_resources(WTFMove(resources))
{
}

BindGroup::BindGroup(Device& device)
    : m_device(device)
{
}

BindGroup::~BindGroup() = default;

void BindGroup::setLabel(String&& label)
{
    auto labelString = label;
    m_vertexArgumentBuffer.label = labelString;
    m_fragmentArgumentBuffer.label = labelString;
    m_computeArgumentBuffer.label = labelString;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuBindGroupReference(WGPUBindGroup bindGroup)
{
    WebGPU::fromAPI(bindGroup).ref();
}

void wgpuBindGroupRelease(WGPUBindGroup bindGroup)
{
    WebGPU::fromAPI(bindGroup).deref();
}

void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, const char* label)
{
    WebGPU::fromAPI(bindGroup).setLabel(WebGPU::fromAPI(label));
}
