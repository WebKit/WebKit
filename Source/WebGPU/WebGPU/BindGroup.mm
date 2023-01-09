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

Ref<BindGroup> Device::createBindGroup(const WGPUBindGroupDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return BindGroup::createInvalid(*this);

    constexpr ShaderStage stages[] = { ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute };
    constexpr size_t stageCount = std::size(stages);
    ShaderStageArray<NSUInteger> bindingIndexForStage = std::array<NSUInteger, stageCount>();
    const auto& bindGroupLayout = WebGPU::fromAPI(descriptor.layout);
    Vector<BindableResource> resources;

    ShaderStageArray<id<MTLArgumentEncoder>> argumentEncoder = std::array<id<MTLArgumentEncoder>, stageCount>({ bindGroupLayout.vertexArgumentEncoder(), bindGroupLayout.fragmentArgumentEncoder(), bindGroupLayout.computeArgumentEncoder() });
    ShaderStageArray<id<MTLBuffer>> argumentBuffer;
    for (ShaderStage stage : stages) {
        auto encodedLength = bindGroupLayout.encodedLength(stage);
        argumentBuffer[stage] = encodedLength ? safeCreateBuffer(encodedLength, MTLStorageModeShared) : nil;
        [argumentEncoder[stage] setArgumentBuffer:argumentBuffer[stage] offset:0];
    }

    for (uint32_t i = 0, entryCount = descriptor.entryCount; i < entryCount; ++i) {
        const WGPUBindGroupEntry& entry = descriptor.entries[i];

        if (entry.nextInChain)
            return BindGroup::createInvalid(*this);

        bool bufferIsPresent = WebGPU::bufferIsPresent(entry);
        bool samplerIsPresent = WebGPU::samplerIsPresent(entry);
        bool textureViewIsPresent = WebGPU::textureViewIsPresent(entry);
        if (bufferIsPresent + samplerIsPresent + textureViewIsPresent != 1)
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
            } else if (samplerIsPresent) {
                id<MTLSamplerState> sampler = WebGPU::fromAPI(entry.sampler).samplerState();
                [argumentEncoder[stage] setSamplerState:sampler atIndex:index++];
            } else if (textureViewIsPresent) {
                id<MTLTexture> texture = WebGPU::fromAPI(entry.textureView).texture();
                [argumentEncoder[stage] setTexture:texture atIndex:index++];
                resources.append({ texture, MTLResourceUsageRead, metalRenderStage(stage) });
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

void wgpuBindGroupRelease(WGPUBindGroup bindGroup)
{
    WebGPU::fromAPI(bindGroup).deref();
}

void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, const char* label)
{
    WebGPU::fromAPI(bindGroup).setLabel(WebGPU::fromAPI(label));
}
