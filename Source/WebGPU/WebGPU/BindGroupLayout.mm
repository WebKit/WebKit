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
#import "BindGroupLayout.h"

#import "APIConversions.h"
#import "Device.h"

namespace WebGPU {

static bool isPresent(const WGPUBufferBindingLayout& buffer)
{
    return buffer.type != WGPUBufferBindingType_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUBufferBindingLayout& buffer)
{
    if (buffer.nextInChain)
        return nil;

    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypePointer;
    // FIXME: Handle hasDynamicOffset.
    // FIXME: Handle minBindingSize.
    switch (buffer.type) {
    case WGPUBufferBindingType_Uniform:
    case WGPUBufferBindingType_ReadOnlyStorage:
        descriptor.access = MTLArgumentAccessReadOnly;
        break;
    case WGPUBufferBindingType_Storage:
        descriptor.access = MTLArgumentAccessReadWrite;
        break;
    case WGPUBufferBindingType_Undefined:
    case WGPUBufferBindingType_Force32:
        ASSERT_NOT_REACHED();
        return nil;
    }
    return descriptor;
}

static bool isPresent(const WGPUSamplerBindingLayout& sampler)
{
    return sampler.type != WGPUSamplerBindingType_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUSamplerBindingLayout& sampler)
{
    if (sampler.nextInChain)
        return nil;

    UNUSED_PARAM(sampler);
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeSampler;
    descriptor.access = MTLArgumentAccessReadOnly;
    return descriptor;
}

static bool isPresent(const WGPUTextureBindingLayout& texture)
{
    return texture.sampleType != WGPUTextureSampleType_Undefined
        && texture.viewDimension != WGPUTextureViewDimension_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUTextureBindingLayout& texture)
{
    if (texture.nextInChain)
        return nil;

    UNUSED_PARAM(texture);
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    descriptor.access = MTLArgumentAccessReadOnly;
    return descriptor;
}

static bool isPresent(const WGPUStorageTextureBindingLayout& storageTexture)
{
    return storageTexture.access != WGPUStorageTextureAccess_Undefined
        && storageTexture.format != WGPUTextureFormat_Undefined
        && storageTexture.viewDimension != WGPUTextureViewDimension_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUStorageTextureBindingLayout& storageTexture)
{
    if (storageTexture.nextInChain)
        return nil;

    UNUSED_PARAM(storageTexture);
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    // FIXME: Implement this.
    return descriptor;
}

static void addDescriptor(NSMutableArray<MTLArgumentDescriptor *> *arguments, MTLArgumentDescriptor *descriptor)
{
    MTLArgumentDescriptor *stageDescriptor = [descriptor copy];
    stageDescriptor.index = arguments.count;
    [arguments addObject:stageDescriptor];
}

static bool containsStage(WGPUShaderStageFlags stageBitfield, auto stage)
{
    static_assert(1 == WGPUShaderStage_Vertex && 2 == WGPUShaderStage_Fragment && 4 == WGPUShaderStage_Compute, "Expect WGPUShaderStage to be a bitfield");
    return stageBitfield & (1 << static_cast<uint32_t>(stage));
}

Ref<BindGroupLayout> Device::createBindGroupLayout(const WGPUBindGroupLayoutDescriptor& descriptor)
{
    if (descriptor.nextInChain || !descriptor.entryCount)
        return BindGroupLayout::createInvalid(*this);

    HashMap<uint32_t, WGPUShaderStageFlags> shaderStageForBinding;
    constexpr size_t stageCount = 3;
    static_assert(stageCount == 3, "vertex, fragment, and compute stages supported");
    NSMutableArray<MTLArgumentDescriptor *> *arguments[stageCount];
    for (size_t i = 0; i < stageCount; ++i)
        arguments[i] = [NSMutableArray arrayWithCapacity:descriptor.entryCount];

    for (uint32_t i = 0; i < descriptor.entryCount; ++i) {
        const WGPUBindGroupLayoutEntry& entry = descriptor.entries[i];
        if (entry.nextInChain)
            return BindGroupLayout::createInvalid(*this);

        shaderStageForBinding.add(entry.binding + 1, entry.visibility);
        MTLArgumentDescriptor *descriptor = nil;

        if (isPresent(entry.buffer))
            descriptor = createArgumentDescriptor(entry.buffer);
        else if (isPresent(entry.sampler))
            descriptor = createArgumentDescriptor(entry.sampler);
        else if (isPresent(entry.texture))
            descriptor = createArgumentDescriptor(entry.texture);
        else if (isPresent(entry.storageTexture))
            descriptor = createArgumentDescriptor(entry.storageTexture);

        if (!descriptor)
            return BindGroupLayout::createInvalid(*this);

        for (size_t stage = 0; stage < stageCount; ++stage) {
            if (containsStage(entry.visibility, stage))
                addDescriptor(arguments[stage], descriptor);
        }
    }

    auto label = fromAPI(descriptor.label);
    id<MTLArgumentEncoder> argumentEncoders[stageCount];
    for (size_t stage = 0; stage < stageCount; ++stage) {
        argumentEncoders[stage] = arguments[stage].count ? [m_device newArgumentEncoderWithArguments:arguments[stage]] : nil;
        argumentEncoders[stage].label = label;
        if (arguments[stage].count && !argumentEncoders[stage])
            return BindGroupLayout::createInvalid(*this);
    }

    return BindGroupLayout::create(WTFMove(shaderStageForBinding), argumentEncoders[0], argumentEncoders[1], argumentEncoders[2]);
}

BindGroupLayout::BindGroupLayout(HashMap<uint32_t, WGPUShaderStageFlags>&& shaderStageForBinding, id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder)
    : m_shaderStageForBinding(WTFMove(shaderStageForBinding))
    , m_vertexArgumentEncoder(vertexArgumentEncoder)
    , m_fragmentArgumentEncoder(fragmentArgumentEncoder)
    , m_computeArgumentEncoder(computeArgumentEncoder)
{
}

BindGroupLayout::BindGroupLayout()
{
}

BindGroupLayout::~BindGroupLayout() = default;

void BindGroupLayout::setLabel(String&& label)
{
    auto labelString = label;
    m_vertexArgumentEncoder.label = labelString;
    m_fragmentArgumentEncoder.label = labelString;
    m_computeArgumentEncoder.label = labelString;
}

NSUInteger BindGroupLayout::encodedLength(ShaderStage shaderStage) const
{
    switch (shaderStage) {
    case ShaderStage::Vertex:
        return m_vertexArgumentEncoder.encodedLength;
    case ShaderStage::Fragment:
        return m_fragmentArgumentEncoder.encodedLength;
    case ShaderStage::Compute:
        return m_computeArgumentEncoder.encodedLength;
    }
}

bool BindGroupLayout::bindingContainsStage(uint32_t bindingIndex, ShaderStage shaderStage) const
{
    ASSERT(m_shaderStageForBinding.contains(bindingIndex + 1));
    return containsStage(m_shaderStageForBinding.find(bindingIndex + 1)->value, shaderStage);
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout)
{
    WebGPU::fromAPI(bindGroupLayout).deref();
}

void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, const char* label)
{
    WebGPU::fromAPI(bindGroupLayout).setLabel(WebGPU::fromAPI(label));
}
