/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#if HAVE(TIER2_ARGUMENT_BUFFERS)
static auto sizeOfEntries(const WGPUBindGroupDescriptor& descriptor, BindGroupLayout* bindGroupLayout)
{
    uint32_t sizes[] = { 0, 0, 0 };
    for (uint32_t entryIndex = 0; entryIndex < descriptor.entryCount; ++entryIndex) {
        const WGPUBindGroupEntry& entry = descriptor.entries[entryIndex];

        auto stages = bindGroupLayout ? bindGroupLayout->stagesForBinding(entry.binding) : (WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute);
        constexpr WGPUShaderStage shaderStage[] = { WGPUShaderStage_Vertex, WGPUShaderStage_Fragment, WGPUShaderStage_Compute };
        bool bufferIsPresent = WebGPU::bufferIsPresent(entry);
        bool samplerIsPresent = WebGPU::samplerIsPresent(entry);
        bool textureViewIsPresent = WebGPU::textureViewIsPresent(entry);

        for (size_t currentStage = 0; currentStage < WTF_ARRAY_LENGTH(shaderStage); ++currentStage) {
            WGPUShaderStage renderStage = shaderStage[currentStage];
            if (!(stages & renderStage))
                continue;

            if (bufferIsPresent)
                sizes[currentStage] += sizeof(float*);
            else if (samplerIsPresent || textureViewIsPresent)
                sizes[currentStage] += sizeof(MTLResourceID);
        }
    }

    return std::array<uint32_t, 3>( { sizes[0], sizes[1], sizes[2] } );
}
#endif

Ref<BindGroup> Device::createBindGroup(const WGPUBindGroupDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return BindGroup::createInvalid(*this);

    Vector<BindableResource> resources;
#if HAVE(TIER2_ARGUMENT_BUFFERS)
    if ([m_device argumentBuffersSupport] != MTLArgumentBuffersTier1) {
        uint32_t entryCount = descriptor.entryCount;
        auto bindGroupLayout = descriptor.layout ? &WebGPU::fromAPI(descriptor.layout) : nullptr;
        auto bufferSizes = sizeOfEntries(descriptor, bindGroupLayout);
        constexpr WGPUShaderStage shaderStage[] = { WGPUShaderStage_Vertex, WGPUShaderStage_Fragment, WGPUShaderStage_Compute };
        constexpr auto shaderStageLength = WTF_ARRAY_LENGTH(shaderStage);
        id<MTLBuffer> argumentBuffer[shaderStageLength];
        for (size_t i = 0; i < shaderStageLength; ++i)
            argumentBuffer[i] = bufferSizes[i] ? safeCreateBuffer(bufferSizes[i], MTLStorageModeShared) : nil;

        char* argumentBufferContents[] = { (char*)argumentBuffer[0].contents, (char*)argumentBuffer[1].contents, (char*)argumentBuffer[2].contents };
        size_t bindingOffsetForStage[] = { 0, 0, 0 };
        for (uint32_t i = 0; i < entryCount; ++i) {
            const WGPUBindGroupEntry& entry = descriptor.entries[i];

            if (entry.nextInChain)
                return BindGroup::createInvalid(*this);

            bool bufferIsPresent = WebGPU::bufferIsPresent(entry);
            bool samplerIsPresent = WebGPU::samplerIsPresent(entry);
            bool textureViewIsPresent = WebGPU::textureViewIsPresent(entry);
            if (bufferIsPresent + samplerIsPresent + textureViewIsPresent != 1)
                return BindGroup::createInvalid(*this);

            auto stages = bindGroupLayout ? bindGroupLayout->stagesForBinding(entry.binding) : (WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute);
            for (size_t currentStage = 0; currentStage < shaderStageLength; ++currentStage) {
                WGPUShaderStage renderStage = shaderStage[currentStage];
                if (!(stages & renderStage))
                    continue;

                auto& offset = bindingOffsetForStage[currentStage];
                ASSERT(argumentBufferContents[currentStage]);

                if (bufferIsPresent) {
                    id<MTLBuffer> buffer = WebGPU::fromAPI(entry.buffer).buffer();
                    if (entry.offset > buffer.length)
                        return BindGroup::createInvalid(*this);

                    ASSERT(sizeof(float*) == sizeof(buffer.gpuAddress));
                    *(float**)(argumentBufferContents[currentStage] + offset) = (float*)((char*)buffer.gpuAddress + entry.offset);
                    offset += sizeof(MTLResourceID);
                } else if (samplerIsPresent) {
                    id<MTLSamplerState> sampler = WebGPU::fromAPI(entry.sampler).samplerState();
                    ASSERT(sizeof(float*) == sizeof(sampler.gpuResourceID));
                    *(MTLResourceID*)(argumentBufferContents[currentStage] + offset) = sampler.gpuResourceID;
                    offset += sizeof(MTLResourceID);
                } else if (textureViewIsPresent) {
                    id<MTLTexture> texture = WebGPU::fromAPI(entry.textureView).texture();
                    ASSERT(sizeof(float*) == sizeof(texture.gpuResourceID));
                    *(MTLResourceID*)(argumentBufferContents[currentStage] + offset) = texture.gpuResourceID;
                    resources.append({ texture, MTLResourceUsageRead, renderStage });
                    offset += sizeof(MTLResourceID);
                }
            }
        }

        return BindGroup::create(argumentBuffer[0], argumentBuffer[1], argumentBuffer[2], WTFMove(resources), *this);
    }
#endif // HAVE(TIER2_ARGUMENT_BUFFERS)

    // FIXME: Validate this according to the spec.

    const BindGroupLayout& bindGroupLayout = WebGPU::fromAPI(descriptor.layout);

    // FIXME(PERFORMANCE): Don't allocate 3 new buffers for every bind group.
    // In fact, don't even allocate a single new buffer for every bind group.
    id<MTLBuffer> vertexArgumentBuffer = safeCreateBuffer(bindGroupLayout.encodedLength(), MTLStorageModeShared);
    id<MTLBuffer> fragmentArgumentBuffer = safeCreateBuffer(bindGroupLayout.encodedLength(), MTLStorageModeShared);
    id<MTLBuffer> computeArgumentBuffer = safeCreateBuffer(bindGroupLayout.encodedLength(), MTLStorageModeShared);
    if (!vertexArgumentBuffer || !fragmentArgumentBuffer || !computeArgumentBuffer)
        return BindGroup::createInvalid(*this);

    auto label = fromAPI(descriptor.label);
    vertexArgumentBuffer.label = label;
    fragmentArgumentBuffer.label = label;
    computeArgumentBuffer.label = label;

    id<MTLArgumentEncoder> vertexArgumentEncoder = bindGroupLayout.vertexArgumentEncoder();
    id<MTLArgumentEncoder> fragmentArgumentEncoder = bindGroupLayout.fragmentArgumentEncoder();
    id<MTLArgumentEncoder> computeArgumentEncoder = bindGroupLayout.computeArgumentEncoder();
    [vertexArgumentEncoder setArgumentBuffer:vertexArgumentBuffer offset:0];
    [fragmentArgumentEncoder setArgumentBuffer:fragmentArgumentBuffer offset:0];
    [computeArgumentEncoder setArgumentBuffer:computeArgumentBuffer offset:0];

    for (uint32_t i = 0; i < descriptor.entryCount; ++i) {
        const WGPUBindGroupEntry& entry = descriptor.entries[i];

        if (entry.nextInChain)
            return BindGroup::createInvalid(*this);

        bool bufferIsPresent = WebGPU::bufferIsPresent(entry);
        bool samplerIsPresent = WebGPU::samplerIsPresent(entry);
        bool textureViewIsPresent = WebGPU::textureViewIsPresent(entry);
        if (static_cast<int>(bufferIsPresent) + static_cast<int>(samplerIsPresent) + static_cast<int>(textureViewIsPresent) != 1)
            return BindGroup::createInvalid(*this);

        if (bufferIsPresent) {
            id<MTLBuffer> buffer = WebGPU::fromAPI(entry.buffer).buffer();
            [vertexArgumentEncoder setBuffer:buffer offset:static_cast<NSUInteger>(entry.offset) atIndex:entry.binding];
            [fragmentArgumentEncoder setBuffer:buffer offset:static_cast<NSUInteger>(entry.offset) atIndex:entry.binding];
            [computeArgumentEncoder setBuffer:buffer offset:static_cast<NSUInteger>(entry.offset) atIndex:entry.binding];
        } else if (samplerIsPresent) {
            id<MTLSamplerState> sampler = WebGPU::fromAPI(entry.sampler).samplerState();
            [vertexArgumentEncoder setSamplerState:sampler atIndex:entry.binding];
            [fragmentArgumentEncoder setSamplerState:sampler atIndex:entry.binding];
            [computeArgumentEncoder setSamplerState:sampler atIndex:entry.binding];
        } else if (textureViewIsPresent) {
            id<MTLTexture> texture = WebGPU::fromAPI(entry.textureView).texture();
            [vertexArgumentEncoder setTexture:texture atIndex:entry.binding];
            [fragmentArgumentEncoder setTexture:texture atIndex:entry.binding];
            [computeArgumentEncoder setTexture:texture atIndex:entry.binding];
        } else {
            ASSERT_NOT_REACHED();
            return BindGroup::createInvalid(*this);
        }
    }

    return BindGroup::create(vertexArgumentBuffer, fragmentArgumentBuffer, computeArgumentBuffer, WTFMove(resources), *this);
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

void wgpuBindGroupRelease(WGPUBindGroup bindGroup)
{
    WebGPU::fromAPI(bindGroup).deref();
}

void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, const char* label)
{
    WebGPU::fromAPI(bindGroup).setLabel(WebGPU::fromAPI(label));
}
