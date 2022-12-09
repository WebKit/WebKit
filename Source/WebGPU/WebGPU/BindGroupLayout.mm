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
    // FIXME: Implement this.
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
    // FIXME: Implement this.
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

Ref<BindGroupLayout> Device::createBindGroupLayout(const WGPUBindGroupLayoutDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return BindGroupLayout::createInvalid(*this);

#if HAVE(TIER2_ARGUMENT_BUFFERS)
    if ([m_device argumentBuffersSupport] != MTLArgumentBuffersTier1) {
        HashMap<uint32_t, uint32_t> shaderStageForBinding;

        NSUInteger vertexArgumentSize = 0;
        NSUInteger fragmentArgumentSize = 0;
        NSUInteger computeArgumentSize = 0;
        constexpr auto uniformBufferElementSize = sizeof(float*);
        for (uint32_t i = 0; i < descriptor.entryCount; ++i) {
            const WGPUBindGroupLayoutEntry& entry = descriptor.entries[i];
            if (entry.nextInChain)
                return BindGroupLayout::createInvalid(*this);

            vertexArgumentSize += (entry.visibility & WGPUShaderStage_Vertex) ? uniformBufferElementSize : 0;
            fragmentArgumentSize += (entry.visibility & WGPUShaderStage_Fragment) ? uniformBufferElementSize : 0;
            computeArgumentSize += (entry.visibility & WGPUShaderStage_Compute) ? uniformBufferElementSize : 0;
            shaderStageForBinding.add(entry.binding + 1, entry.visibility);
        }

        id<MTLBuffer> vertexArgumentBuffer = vertexArgumentSize ? safeCreateBuffer(vertexArgumentSize, MTLStorageModeShared) : nil;
        id<MTLBuffer> fragmentArgumentBuffer = fragmentArgumentSize ? safeCreateBuffer(fragmentArgumentSize, MTLStorageModeShared) : nil;
        id<MTLBuffer> computeArgumentBuffer = computeArgumentSize ? safeCreateBuffer(computeArgumentSize, MTLStorageModeShared) : nil;

        auto label = fromAPI(descriptor.label);
        vertexArgumentBuffer.label = label;
        fragmentArgumentBuffer.label = label;
        computeArgumentBuffer.label = label;

        return BindGroupLayout::create(vertexArgumentBuffer, fragmentArgumentBuffer, computeArgumentBuffer, WTFMove(shaderStageForBinding), *this);
    }
#endif // HAVE(TIER2_ARGUMENT_BUFFERS)

    NSMutableArray<MTLArgumentDescriptor *> *vertexArguments = [NSMutableArray arrayWithCapacity:descriptor.entryCount];
    NSMutableArray<MTLArgumentDescriptor *> *fragmentArguments = [NSMutableArray arrayWithCapacity:descriptor.entryCount];
    NSMutableArray<MTLArgumentDescriptor *> *computeArguments = [NSMutableArray arrayWithCapacity:descriptor.entryCount];
    if (!vertexArguments || !fragmentArguments || !computeArguments)
        return BindGroupLayout::createInvalid(*this);

    for (uint32_t i = 0; i < descriptor.entryCount; ++i) {
        const WGPUBindGroupLayoutEntry& entry = descriptor.entries[i];
        if (entry.nextInChain)
            return BindGroupLayout::createInvalid(*this);

        // https://gpuweb.github.io/gpuweb/#dictdef-gpubindgrouplayoutentry
        // "Only one [of buffer, sampler, texture, storageTexture, or externalTexture] may be defined for any given GPUBindGroupLayoutEntry."

        ASSERT(i == vertexArguments.count);
        ASSERT(vertexArguments.count == fragmentArguments.count);
        ASSERT(fragmentArguments.count == computeArguments.count);

        const WGPUBufferBindingLayout& buffer = entry.buffer;
        const WGPUSamplerBindingLayout& sampler = entry.sampler;
        const WGPUTextureBindingLayout& texture = entry.texture;
        const WGPUStorageTextureBindingLayout& storageTexture = entry.storageTexture;

        MTLArgumentDescriptor *descriptor = nil;

        if (isPresent(buffer)) {
            if (i != vertexArguments.count)
                return BindGroupLayout::createInvalid(*this);

            descriptor = createArgumentDescriptor(buffer);
            if (!descriptor)
                return BindGroupLayout::createInvalid(*this);
        }

        if (isPresent(sampler)) {
            if (i != vertexArguments.count)
                return BindGroupLayout::createInvalid(*this);

            descriptor = createArgumentDescriptor(sampler);
            if (!descriptor)
                return BindGroupLayout::createInvalid(*this);
        }

        if (isPresent(texture)) {
            if (i != vertexArguments.count)
                return BindGroupLayout::createInvalid(*this);

            descriptor = createArgumentDescriptor(texture);
            if (!descriptor)
                return BindGroupLayout::createInvalid(*this);
        }

        if (isPresent(storageTexture)) {
            if (i != vertexArguments.count)
                return BindGroupLayout::createInvalid(*this);

            descriptor = createArgumentDescriptor(storageTexture);
            if (!descriptor)
                return BindGroupLayout::createInvalid(*this);
        }

        // FIXME: This needs coordination with the shader compiler.
        descriptor.index = i;

        MTLArgumentDescriptor *vertexDescriptor = descriptor;
        MTLArgumentDescriptor *fragmentDescriptor = [descriptor copy];
        MTLArgumentDescriptor *computeDescriptor = [descriptor copy];
        if (!fragmentDescriptor || !computeDescriptor)
            return BindGroupLayout::createInvalid(*this);

        if (!(entry.visibility & WGPUShaderStage_Vertex))
            vertexDescriptor.access = MTLArgumentAccessReadOnly;
        if (!(entry.visibility & WGPUShaderStage_Fragment))
            fragmentDescriptor.access = MTLArgumentAccessReadOnly;
        if (!(entry.visibility & WGPUShaderStage_Compute))
            computeDescriptor.access = MTLArgumentAccessReadOnly;

        [vertexArguments addObject:vertexDescriptor];
        [fragmentArguments addObject:fragmentDescriptor];
        [computeArguments addObject:computeDescriptor];
    }

    id<MTLArgumentEncoder> vertexArgumentEncoder = [m_device newArgumentEncoderWithArguments:vertexArguments];
    id<MTLArgumentEncoder> fragmentArgumentEncoder = [m_device newArgumentEncoderWithArguments:fragmentArguments];
    id<MTLArgumentEncoder> computeArgumentEncoder = [m_device newArgumentEncoderWithArguments:computeArguments];
    if (!vertexArgumentEncoder || !fragmentArgumentEncoder || !computeArgumentEncoder)
        return BindGroupLayout::createInvalid(*this);

    auto label = fromAPI(descriptor.label);
    vertexArgumentEncoder.label = label;
    fragmentArgumentEncoder.label = label;
    computeArgumentEncoder.label = label;

    return BindGroupLayout::create(vertexArgumentEncoder, fragmentArgumentEncoder, computeArgumentEncoder, *this);
}

BindGroupLayout::BindGroupLayout(id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, Device& device)
    : m_vertexArgumentEncoder(vertexArgumentEncoder)
    , m_fragmentArgumentEncoder(fragmentArgumentEncoder)
    , m_computeArgumentEncoder(computeArgumentEncoder)
    , m_device(device)
{
}

BindGroupLayout::BindGroupLayout(id<MTLBuffer> vertexArgumentBuffer, id<MTLBuffer> fragmentArgumentBuffer, id<MTLBuffer> computeArgumentBuffer, HashMap<uint32_t, uint32_t>&& shaderStageForBinding, Device& device)
    : m_vertexArgumentBuffer(vertexArgumentBuffer)
    , m_fragmentArgumentBuffer(fragmentArgumentBuffer)
    , m_computeArgumentBuffer(computeArgumentBuffer)
    , m_device(device)
    , m_shaderStageForBinding(WTFMove(shaderStageForBinding))
{
}

BindGroupLayout::BindGroupLayout(Device& device)
    : m_device(device)
{
}

BindGroupLayout::~BindGroupLayout()
{
}

void BindGroupLayout::setLabel(String&& label)
{
    auto labelString = label;
    m_vertexArgumentEncoder.label = labelString;
    m_fragmentArgumentEncoder.label = labelString;
    m_computeArgumentEncoder.label = labelString;
}

NSUInteger BindGroupLayout::encodedLength() const
{
    auto result = m_vertexArgumentEncoder.encodedLength;
    ASSERT(result == m_fragmentArgumentEncoder.encodedLength);
    ASSERT(result == m_computeArgumentEncoder.encodedLength);
    return result;
}

uint32_t BindGroupLayout::stageForBinding(uint32_t binding) const
{
    ASSERT(m_shaderStageForBinding.contains(binding + 1));
    return m_shaderStageForBinding.find(binding + 1)->value;
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
